#include "mupdf/fitz.h"
#include "mupdf/pdf.h"

static void pdf_overlay_page(fz_context *ctx, pdf_document *doc_base, pdf_obj *page_base, pdf_document *doc_text, pdf_obj *page_text, pdf_graft_map *graft_map, float scale_text)
{
	pdf_obj *res_base;
	pdf_obj *res_text;
	pdf_obj *res_base_fonts;
	pdf_obj *res_text_fonts;
	pdf_obj *font;
	pdf_obj *font_graft;
	pdf_obj *res_text_fonts_graft;
	pdf_obj *res_text_gstate;
	pdf_obj *res_text_gstate_graft;
	pdf_obj *res_base_gstate;
	pdf_obj *res_base_gstate_graft;
	pdf_obj *contents_base;
	pdf_obj *contents_text;
	pdf_obj *contents_text_graft;
	pdf_obj *new_contents = NULL;
	pdf_obj *obj_q1 = NULL;
	pdf_obj *obj_q2 = NULL;
	pdf_obj *obj_Q = NULL;
	fz_buffer *buf = NULL;
	fz_buffer *buf2 = NULL;
	fz_buffer *buf3 = NULL;
	pdf_obj *key, *val;
	int prepend, append;
	int i;
	int n;

	fz_var(buf);
	fz_var(buf2);
	fz_var(buf3);
	fz_var(obj_q1);
	fz_var(obj_q2);
	fz_var(obj_Q);
	fz_var(new_contents);

	res_base = pdf_dict_get(ctx, page_base, PDF_NAME(Resources));
	if (!res_base)
		res_base = pdf_dict_put_dict(ctx, page_base, PDF_NAME(Resources), 4);

	fz_try(ctx)
	{
		res_text = pdf_dict_get(ctx, page_text, PDF_NAME(Resources));
		if (!res_text)
			res_text = pdf_dict_put_dict(ctx, page_text, PDF_NAME(Resources), 4);

		// Ensure that the graphics state is balanced.
		contents_base = pdf_dict_get(ctx, page_base, PDF_NAME(Contents));
		contents_text = pdf_dict_get(ctx, page_text, PDF_NAME(Contents));

		buf = fz_new_buffer(ctx, 1024);
		fz_append_string(ctx, buf, "q\n");
		obj_q1 = pdf_add_stream(ctx, doc_base, buf, NULL, 0);
		fz_drop_buffer(ctx, buf);
		buf = NULL;

		buf2 = fz_new_buffer(ctx, 1024);
		fz_append_string(ctx, buf2, "Q\n");
		obj_Q = pdf_add_stream(ctx, doc_base, buf2, NULL, 0);
		fz_drop_buffer(ctx, buf2);
		buf2 = NULL;

		buf3 = fz_new_buffer(ctx, 1024);
		fz_append_printf(ctx, buf3, "q %g 0 0 %g 0 0 cm\n", scale_text, scale_text);

		obj_q2 = pdf_add_stream(ctx, doc_base, buf3, NULL, 0);
		fz_drop_buffer(ctx, buf3);
		buf3 = NULL;

		contents_text_graft = pdf_graft_object(ctx, doc_base, contents_text);

		if (!pdf_is_array(ctx, contents_base))
		{
			new_contents = pdf_new_array(ctx, doc_base, 10);
			pdf_array_push(ctx, new_contents, obj_q1);
			pdf_array_push(ctx, new_contents, contents_base);
			pdf_array_push(ctx, new_contents, obj_Q);
			pdf_dict_put(ctx, page_base, PDF_NAME(Contents), new_contents);
			pdf_drop_obj(ctx, new_contents);
			contents_base = new_contents;
			new_contents = NULL;
		}

		pdf_array_push(ctx, contents_base, obj_q2);
		pdf_array_push_drop(ctx, contents_base, contents_text_graft);
		pdf_array_push(ctx, contents_base, obj_Q);

		// res_text_fonts = pdf_dict_get(ctx, res_text, PDF_NAME(Font));
		// if (res_text_fonts) {
		// 	res_text_fonts_graft = pdf_graft_mapped_object(ctx, graft_map, res_text_fonts);
		// 	pdf_dict_put_drop(ctx, res_base, PDF_NAME(Font), res_text_fonts_graft);
		// }

		res_base_fonts = pdf_dict_get(ctx, res_base, PDF_NAME(Font));
		res_text_fonts = pdf_dict_get(ctx, res_text, PDF_NAME(Font));
		if (res_text_fonts) {

			if (!res_base_fonts)
			{
				res_base_fonts =  pdf_dict_put_dict(ctx, res_base, PDF_NAME(Font), 1);
			}

			n = pdf_dict_len(ctx, res_text_fonts);
			for (i = 0; i < n; i++)
			{
				font = pdf_dict_get_val(ctx, res_text_fonts, i);
				key = pdf_dict_get_key(ctx, res_text_fonts, i);
				font_graft = pdf_graft_mapped_object(ctx, graft_map, font);
				pdf_dict_put_drop(ctx, res_base_fonts, key, font_graft);
				// fz_write_printf(ctx, fz_stderr(ctx), "Adding new font: %s\n", pdf_to_name(ctx, key));
				// pdf_dict_put_drop(ctx, dict, pdf_dict_get_key(ctx, obj, i), obj_copy);
			}
		}

		
		res_base_gstate = pdf_dict_get(ctx, res_base, PDF_NAME(ExtGState));
		res_text_gstate = pdf_dict_get(ctx, res_text, PDF_NAME(ExtGState));
		if (res_text_gstate) {

			if (!res_base_gstate)
			{
				res_base_gstate =  pdf_dict_put_dict(ctx, res_base, PDF_NAME(ExtGState), 1);
			}

			n = pdf_dict_len(ctx, res_text_gstate);
			for (i = 0; i < n; i++)
			{
				font = pdf_dict_get_val(ctx, res_text_gstate, i);
				key = pdf_dict_get_key(ctx, res_text_gstate, i);
				font_graft = pdf_graft_mapped_object(ctx, graft_map, font);
				pdf_dict_put_drop(ctx, res_base_gstate, key, font_graft);
				// fz_write_printf(ctx, fz_stderr(ctx), "Adding new gstate: %s\n", pdf_to_name(ctx, key));
				// pdf_dict_put_drop(ctx, dict, pdf_dict_get_key(ctx, obj, i), obj_copy);
			}
		}


		// if (res_text_gstate) {
		// 	res_text_gstate_graft = pdf_graft_mapped_object(ctx, graft_map, res_text_gstate);
		// 	pdf_dict_put_drop(ctx, res_base, PDF_NAME(ExtGState), res_text_gstate_graft);
		// }

	}
	fz_always(ctx)
	{
		fz_drop_buffer(ctx, buf);
		pdf_drop_obj(ctx, obj_q1);
		pdf_drop_obj(ctx, obj_q2);
		pdf_drop_obj(ctx, obj_Q);
		pdf_drop_obj(ctx, new_contents);
	}
	fz_catch(ctx)
	{
		fz_rethrow(ctx);
	}
}


void pdf_overlay_documents(fz_context *ctx, pdf_document *doc_base, pdf_document *doc_text)
{
	pdf_page *page_base = NULL;
	pdf_page *page_text = NULL;
	pdf_graft_map *graft_map;
	fz_rect mediabox_base;
	fz_rect mediabox_text;
	int i, n;

	fz_var(page_base);
	fz_var(page_text);

	pdf_begin_operation(ctx, doc_base, "Bake interactive content");
	fz_try(ctx)
	{

		graft_map = pdf_new_graft_map(ctx, doc_base);

		n = pdf_count_pages(ctx, doc_base);
		for (i = 0; i < n; ++i)
		{
			page_base = pdf_load_page(ctx, doc_base, i);
			page_text = pdf_load_page(ctx, doc_text, i);

			mediabox_base = fz_bound_page(ctx, page_base);
			mediabox_text = fz_bound_page(ctx, page_text);

			float scale_x = mediabox_base.x1 / mediabox_text.x1;

			pdf_overlay_page(ctx, doc_base, page_base->obj, doc_text, page_text->obj, graft_map, scale_x);

		}

		pdf_end_operation(ctx, doc_base);
	}
	fz_always(ctx)
	{
		pdf_drop_graft_map(ctx, graft_map);
		fz_drop_page(ctx, (fz_page*)page_base);
		fz_drop_page(ctx, (fz_page*)page_text);
	}
	fz_catch(ctx)
	{
		pdf_abandon_operation(ctx, doc_base);
	}
}
