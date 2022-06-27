// Copyright (C) 2004-2022 Artifex Software, Inc.
//
// This file is part of MuPDF.
//
// MuPDF is free software: you can redistribute it and/or modify it under the
// terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// MuPDF is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
// details.
//
// You should have received a copy of the GNU Affero General Public License
// along with MuPDF. If not, see <https://www.gnu.org/licenses/agpl-3.0.en.html>
//
// Alternative licensing terms are available from the licensor.
// For commercial licensing, see <https://www.artifex.com/> or contact
// Artifex Software, Inc., 1305 Grant Avenue - Suite 200, Novato,
// CA 94945, U.S.A., +1(415)492-9861, for further information.

/*global mupdf */

"use strict";

// Import the WASM module
importScripts("libmupdf.js");
importScripts("lib/mupdf.js");

mupdf.ready.then(() => {
	postMessage(["READY"]);
});

onmessage = async function (event) {
	let [ func, id, args ] = event.data;
	await mupdf.ready;

	try {
		let result = workerMethods[func](...args);
		postMessage(["RESULT", id, result]);
	} catch (error) {
		if (error instanceof mupdf.MupdfTryLaterError) {
			//trylaterQueue.push(event);
			console.error("TRYLATER ERROR");
		} else {
			console.error(`${error.name} calling ${func}: ${error.message}\n${error.stack}`);
			postMessage(["ERROR", id, {name: error.name, message: error.message}]);
		}
	}
};

const workerMethods = {};

let openDocument = null;

workerMethods.openDocumentFromBuffer = function (buffer, magic) {
	// TODO - check types
	openDocument = mupdf.Document.openFromData(buffer, magic);
};

workerMethods.openDocumentFromUrl = function (url, contentLength, progressive, prefetch, magic) {
	let stream = mupdf.Stream.fromUrl(url, contentLength, Math.max(progressive << 10, 1 << 16), prefetch);
	// TODO - close stream?
	openDocument = mupdf.Document.openFromStream(stream, magic);
};

workerMethods.freeDocument = function () {
	openDocument?.free();
	openDocument = null;
};

workerMethods.documentTitle = function () {
	return openDocument.title();
};

workerMethods.documentOutline = function () {
	const root = openDocument.loadOutline();

	if (root == null)
		return null;

	function makeOutline(node) {
		let list = [];
		while (node) {
			let entry = {
				title: node.title(),
				page: node.pageNumber(openDocument),
			};
			let down = node.down();
			if (down)
				entry.down = makeOutline(down);
			list.push(entry);
			node = node.next();
		}
		return list;
	}

	try {
		return makeOutline(root);
	} finally {
		root.free();
	}
};

workerMethods.countPages = function() {
	return openDocument.countPages();
};

// TODO - currently this loads every single page. Not very efficient?
workerMethods.getPageSizes = function (dpi) {
	let list = [];
	let n = openDocument.countPages();
	for (let i = 1; i <= n; ++i) {
		let page;
		try {
			page = openDocument.loadPage(i);
			let width = page.width() * dpi / 72;
			let height = page.height() * dpi / 72;
			list.push({width, height});
		}
		finally {
			page.free();
		}
	}
	return list;
};

// TODO - keep page loaded?
workerMethods.getPageWidth = function (pageNumber, dpi) {
	let page = openDocument.loadPage(pageNumber);
	return page.width() * dpi / 72;
};

workerMethods.getPageHeight = function (pageNumber, dpi) {
	let page = openDocument.loadPage(pageNumber);
	return page.height() * dpi / 72;
};

workerMethods.getPageLinks = function(pageNumber, dpi) {
	const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72);
	let page;
	let links_ptr;

	try {
		page = openDocument.loadPage(pageNumber);
		links_ptr = page.loadLinks();

		return links_ptr.links.map(link => {
			const { x0, y0, x1, y1 } = doc_to_screen.transformRect(link.rect());

			let href;
			if (link.isExternalLink()) {
				href = link.uri();
			} else {
				const linkPageNumber = link.resolve(openDocument).pageNumber(openDocument);
				// TODO - document the "+ 1" better
				href = `#page${linkPageNumber + 1}`;
			}

			return {
				x: x0,
				y: y0,
				w: x1 - x0,
				h: y1 - y0,
				href
			};
		});
	}
	finally {
		page?.free();
		links_ptr?.free();
	}
};

workerMethods.getPageText = function(pageNumber, dpi) {
	let page;
	let stextPage;

	let buffer;
	let output;

	try {
		page = openDocument.loadPage(pageNumber);
		stextPage = page.toSTextPage();

		buffer = mupdf.Buffer.empty();
		output = mupdf.Output.withBuffer(buffer);

		stextPage.printAsJson(output, dpi / 72);
		output.close();

		let text = buffer.toJsString();
		return JSON.parse(text);
	}
	finally {
		output?.free();
		buffer?.free();
		stextPage?.free();
		page?.free();
	}
};

workerMethods.search = function(pageNumber, dpi, needle) {
	let page;

	try {
		page = openDocument.loadPage(pageNumber);
		const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72);
		const hits = page.search(needle);
		return hits.map(searchHit => {
			const  { x0, y0, x1, y1 } = doc_to_screen.transformRect(searchHit);

			return {
				x: x0,
				y: y0,
				w: x1 - x0,
				h: y1 - y0,
			};
		});
	}
	finally {
		page?.free();
	}
};

workerMethods.getPageAnnotations = function(pageNumber, dpi) {
	let pdfPage;

	try {
		pdfPage = openDocument.loadPage(pageNumber);

		if (pdfPage == null) {
			return [];
		}

		const annotations = pdfPage.annotations();
		const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72);

		return annotations.annotations.map(annotation => {
			const { x0, y0, x1, y1 } = doc_to_screen.transformRect(annotation.bounds());

			return {
				x: x0,
				y: y0,
				w: x1 - x0,
				h: y1 - y0,
				type: annotation.annotType(),
				ref: annotation.pointer,
			};
		});
	}
	finally {
		pdfPage?.free();
	}
};

workerMethods.drawPageAsPNG = function(pageNumber, dpi) {
	const doc_to_screen = mupdf.Matrix.scale(dpi / 72, dpi / 72);
	let page;
	let pixmap;

	// TODO - draw annotations
	// TODO - use canvas?

	try {
		page = openDocument.loadPage(pageNumber);
		pixmap = page.toPixmap(doc_to_screen, mupdf.DeviceRGB, false);
		let png = pixmap.toPNG();
		return png;
	}
	finally {
		pixmap?.free();
		page?.free();
	}
};
