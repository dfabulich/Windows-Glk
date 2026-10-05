/*
   Header file for Windows specific Glk features.
   Glk API version 0.7.6, WinGlk release 1.55.
*/

#ifndef WINGLK_H_
#define WINGLK_H_

#include <wtypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WINGLK_BUILD_NUMBER 155

/* Function to be implemented in the Glk program. */
int winglk_startup_code(const char* cmdline);

/* Initialize Windows Glk, or fail with an error message. */
int InitGlk(unsigned int version);

/* Windows Glk specific functions. */
strid_t winglk_stream_open_resource(const char* name, const char* type, glui32 rock);
void winglk_app_set_name(const char* name);
void winglk_window_set_title(const char* title);
void winglk_set_resource_directory(const char* dir);
const char* winglk_get_initial_filename(const char* cmdline, const char* title, const char* filter);
void winglk_set_gui(unsigned int id);
void winglk_load_config_file(const char* gamename);
void* winglk_get_resource_handle(void);
void winglk_set_about_text(const char* text);
void winglk_set_menu_name(const char* name);
void winglk_set_help_file(const char* filename);
frefid_t winglk_fileref_create_by_name(glui32 usage, char *name, glui32 rock, int validate);
void winglk_show_game_dialog(void);

/* Windows Glk specific events. */
#define winglk_evtype_GuiInput (0x80000000)

/* Somewhat official Unix extensions. */
const char *glkunix_fileref_get_filename(frefid_t fref);

/* Unofficial Glk extensions. */
void sglk_set_basename(char *s);

/* Gargoyle Glk extensions. */

#define GLK_MODULE_GARGLKTEXT
#define gestalt_GarglkText (0x1100)

void garglk_set_zcolors(glui32 fg, glui32 bg);
void garglk_set_zcolors_stream(strid_t str, glui32 fg, glui32 bg);
void garglk_set_reversevideo(glui32 reverse);
void garglk_set_reversevideo_stream(strid_t str, glui32 reverse);

#define zcolor_Default (-1)
#define zcolor_Current (-2)

/* CSS Glk extension (Dannii Willis), basic profile. */

#define GLK_MODULE_CSS_BASIC
#define GLK_MODULE_CSS_SUPPORTS
#define gestalt_CSSBasic (0x1110)
#define gestalt_WebBrowser (0x1111)
#define gestalt_CSSSupports (0x1119)

#define CSS_Span (0)
#define CSS_Paragraph (1)
#define CSS_Hyperlink (2)
#define CSS_Image (3)
#define CSS_Input (4)
#define CSS_Window (5)

void glk_css_hint_set(glui32 wintype, glui32 csstarget, glui32 style,
  const char *prop, glui32 proplen, const char *val, glui32 vallen);
void glk_css_hint_set_num(glui32 wintype, glui32 csstarget, glui32 style,
  const char *prop, glui32 proplen, glsi32 val);
void glk_css_hint_clear(glui32 wintype, glui32 csstarget, glui32 style,
  const char *prop, glui32 proplen);
void glk_css_inline_set(glui32 csstarget, const char *prop, glui32 proplen,
  const char *val, glui32 vallen);
void glk_css_inline_set_num(glui32 csstarget, const char *prop, glui32 proplen,
  glsi32 val);
void glk_css_inline_clear(glui32 csstarget, const char *prop, glui32 proplen);
void glk_css_hint_clear_all_by_style(glui32 wintype, glui32 style);
void glk_css_hint_clear_all_by_window(glui32 wintype);
void glk_css_hint_clear_all_inline(void);
glui32 glk_css_supports(const char *prop, glui32 proplen,
  const char *val, glui32 vallen);
glui32 glk_css_supports_num(const char *prop, glui32 proplen, glsi32 val);

#ifdef __cplusplus
}
#endif

#endif /* WINGLK_H_ */
