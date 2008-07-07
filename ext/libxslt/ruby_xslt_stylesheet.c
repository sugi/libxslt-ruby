/* $Id: ruby_xslt_stylesheet.c 42 2007-12-07 06:09:35Z transami $ */

/* See the LICENSE file for copyright and distribution information. */

#include "libxslt.h"
#include "ruby_xslt_stylesheet.h"

VALUE cXSLTStylesheet;

/* call-seq: 
 *   sheet.apply => (true|false)
 * 
 * Apply this stylesheet transformation to the source
 * document.
 */
VALUE
ruby_xslt_stylesheet_apply(int argc, VALUE *argv, VALUE self) {
  ruby_xslt_stylesheet *xss;
  ruby_xml_document_t *rxd;
  const char **params;
  VALUE parameter, tmp;
  int i, len;

  Data_Get_Struct(self, ruby_xslt_stylesheet, xss);

  if (NIL_P(xss->xml_doc_obj))
    rb_raise(rb_eArgError, "Need a document object");

  Data_Get_Struct(xss->xml_doc_obj, ruby_xml_document_t, rxd);

  params = NULL;

  switch(argc) {
  case 0:
    break;
  case 1:
    parameter = argv[0];
#if RUBY_VERSION_CODE >= 180
    if (TYPE(parameter) == T_HASH) {
      /* Convert parameter to an array */
      parameter = rb_hash_to_a(parameter);
    }
#endif

    if (TYPE(parameter) == T_ARRAY) {
      /* A hash is better than an array, but we can live with an array of arrays */
      len = RARRAY(parameter)->len;
      params = (void *)ALLOC_N(char *, (len * 2) + 2);
      for (i=0; i < RARRAY(parameter)->len; i++) {
	tmp = RARRAY(parameter)->ptr[i];

	Check_Type(tmp, T_ARRAY);
	Check_Type(RARRAY(tmp)->ptr[0], T_STRING);
	Check_Type(RARRAY(tmp)->ptr[1], T_STRING);

	params[2*i] = RSTRING(RARRAY(tmp)->ptr[0])->ptr;
	params[2*i+1] = RSTRING(RARRAY(tmp)->ptr[1])->ptr;
      }
      params[2*i] = params[2*i+1] = 0;
    } else {
      /* I should test to see if the object responds to to_a and to_h before calling this, but oh well */
      rb_raise(rb_eTypeError, "xslt_stylesheet_appy: expecting a hash or an array of arrays as a parameter");
    }

    break;
  default:
    rb_raise(rb_eArgError, "wrong number of arguments (0 or 1)");
  }

  xss->parsed = ruby_xml_document_wrap(xsltApplyStylesheet(xss->xsp,
							   rxd->doc, params));

  if (params) {
    ruby_xfree(params);
  }

  if (xss->parsed == Qnil)
    return(Qfalse);
  else
    return(Qtrue);
}


/* call-seq: 
 *   sheet.debug(to = $stdout) => (true|false)
 * 
 * Output a debug dump of this stylesheet to the specified output
 * stream (an instance of IO, defaults to $stdout). Requires
 * libxml/libxslt be compiled with debugging enabled. If this
 * is not the case, a warning is triggered and the method returns
 * false.
 */
VALUE
ruby_xslt_stylesheet_debug(int argc, VALUE *argv, VALUE self) {
#ifdef LIBXML_DEBUG_ENABLED
  OpenFile *fptr;
  VALUE io;
  FILE *out;
  ruby_xml_document_t *parsed;
  ruby_xslt_stylesheet *xss;

  Data_Get_Struct(self, ruby_xslt_stylesheet, xss);
  if (NIL_P(xss->parsed))
    rb_raise(eXMLXSLTStylesheetRequireParsedDoc, "must have a parsed XML result");

  switch (argc) {
  case 0:
    io = rb_stdout;
    break;
  case 1:
    io = argv[0];
    if (rb_obj_is_kind_of(io, rb_cIO) == Qfalse)
      rb_raise(rb_eTypeError, "need an IO object");
    break;
  default:
    rb_raise(rb_eArgError, "wrong number of arguments (0 or 1)");
  }

  Data_Get_Struct(xss->parsed, ruby_xml_document_t, parsed);
  if (parsed->doc == NULL)
    return(Qnil);

  GetOpenFile(io, fptr);
  rb_io_check_writable(fptr);
  out = GetWriteFile(fptr);
  xmlDebugDumpDocument(out, parsed->doc);
  return(Qtrue);
#else
  rb_warn("libxml/libxslt was compiled without debugging support.  Please recompile libxml/libxslt and their Ruby modules");
  return(Qfalse);
#endif
}


void
ruby_xslt_stylesheet_free(ruby_xslt_stylesheet *xss) {
  if (xss->xsp != NULL) {
    xsltFreeStylesheet(xss->xsp);
    xss->xsp = NULL;
  }

  ruby_xfree(xss);
}


void
ruby_xslt_stylesheet_mark(ruby_xslt_stylesheet *xss) {
  if (!NIL_P(xss->parsed))      rb_gc_mark(xss->parsed);
  if (!NIL_P(xss->xml_doc_obj)) rb_gc_mark(xss->xml_doc_obj);

  switch (xss->data_type) {
  case RUBY_LIBXSLT_SRC_TYPE_FILE:
    if (xss->data != NULL)
      rb_gc_mark((VALUE)xss->data);
    break;
  }
}


VALUE
ruby_xslt_stylesheet_new(VALUE class, xsltStylesheetPtr xsp) {
  ruby_xslt_stylesheet *xss;
  VALUE rval;

  rval=Data_Make_Struct(cXSLTStylesheet,ruby_xslt_stylesheet,ruby_xslt_stylesheet_mark,
			ruby_xslt_stylesheet_free,xss);
  xss->xsp = xsp;
  xss->xml_doc_obj = Qnil;
  xss->parsed = Qnil;
  xss->data_type = RUBY_LIBXSLT_SRC_TYPE_NULL;
  xss->data = NULL;

  return rval;
}

// TODO should this automatically apply the sheet if not already,
//      given that we're unlikely to do much else with it?

/* call-seq: 
 *   sheet.print(to = $stdout) => number_of_bytes
 * 
 * Output the result of the transform to the specified output
 * stream (an IO instance, defaults to $stdout). You *must* call
 * +apply+ before this method or an exception will be raised.
 */
VALUE
ruby_xslt_stylesheet_print(int argc, VALUE *argv, VALUE self) {
  OpenFile *fptr;
  VALUE io;
  FILE *out;
  ruby_xml_document_t *parsed;
  ruby_xslt_stylesheet *xss;
  int bytes;

  Data_Get_Struct(self, ruby_xslt_stylesheet, xss);
  if (NIL_P(xss->parsed))
    rb_raise(eXMLXSLTStylesheetRequireParsedDoc, "must have a parsed XML result");

  switch (argc) {
  case 0:
    io = rb_stdout;
    break;
  case 1:
    io = argv[0];
    if (rb_obj_is_kind_of(io, rb_cIO) == Qfalse)
      rb_raise(rb_eTypeError, "need an IO object");
    break;
  default:
    rb_raise(rb_eArgError, "wrong number of arguments (0 or 1)");
  }

  Data_Get_Struct(xss->parsed, ruby_xml_document_t, parsed);
  if (parsed->doc == NULL)
    return(Qnil);

  GetOpenFile(io, fptr);
  rb_io_check_writable(fptr);
  out = GetWriteFile(fptr);
  bytes = xsltSaveResultToFile(out, parsed->doc, xss->xsp);

  return(INT2NUM(bytes));
}

// TODO this, too. Either way, to_s probably should have prereqs
//      like this, for one thing it makes IRB use tricky...

/* call-seq: 
 *   sheet.to_s => "result"
 * 
 * Obtain the result of the transform as a string. You *must* call
 * +apply+ before this method or an exception will be raised.
 */
VALUE
ruby_xslt_stylesheet_to_s(VALUE self) {
  ruby_xml_document_t *parsed;
  ruby_xslt_stylesheet *xss;
  xmlChar *str;
  int len;

  Data_Get_Struct(self, ruby_xslt_stylesheet, xss);
  if (NIL_P(xss->parsed))
    rb_raise(eXMLXSLTStylesheetRequireParsedDoc, "must have a parsed XML result");
  Data_Get_Struct(xss->parsed, ruby_xml_document_t, parsed);
  if (parsed->doc == NULL)
    return(Qnil);

  xsltSaveResultToString(&str, &len, parsed->doc, xss->xsp);
  if (str == NULL)
    return(Qnil);
  else
    return(rb_str_new((const char*)str,len));
}



/* call-seq: 
 *   sheet.save(io) => true
 * 
 * Save the result of the transform to the supplied open
 * file (an IO instance). You *must* call +apply+ before 
 * this method or an exception will be raised.
 */
VALUE
ruby_xslt_stylesheet_save(VALUE self, VALUE io) {
  ruby_xml_document_t *parsed;
  ruby_xslt_stylesheet *xss;
  OpenFile *fptr;

  if (rb_obj_is_kind_of(io, rb_cIO) == Qfalse)
    rb_raise(rb_eArgError, "Only accept IO objects for saving");

  GetOpenFile(io, fptr);

  Data_Get_Struct(self, ruby_xslt_stylesheet, xss);
  Data_Get_Struct(xss->parsed, ruby_xml_document_t, parsed);

  xsltSaveResultToFile(fptr->f, parsed->doc, xss->xsp);

  return(Qtrue);
}

#ifdef RDOC_NEVER_DEFINED
  mXML = rb_define_module("XML");
  cXSLT = rb_define_class_under(mXML, "XSLT", rb_cObject);
#endif

void
ruby_init_xslt_stylesheet(void) {
  cXSLTStylesheet = rb_define_class_under(cXSLT, "Stylesheet", rb_cObject);
  eXMLXSLTStylesheetRequireParsedDoc =
    rb_define_class_under(cXSLTStylesheet, "RequireParsedDoc", rb_eException);

  rb_define_method(cXSLTStylesheet, "apply", ruby_xslt_stylesheet_apply, -1);
  rb_define_method(cXSLTStylesheet, "debug", ruby_xslt_stylesheet_debug, -1);
  rb_define_method(cXSLTStylesheet, "print", ruby_xslt_stylesheet_print, -1);
  rb_define_method(cXSLTStylesheet, "to_s", ruby_xslt_stylesheet_to_s, 0);
  rb_define_method(cXSLTStylesheet, "save", ruby_xslt_stylesheet_save, 1);
}
