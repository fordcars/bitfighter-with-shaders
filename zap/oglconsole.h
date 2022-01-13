//------------------------------------------------------------------------------
// Copyright Chris Eykamp
// See LICENSE.txt for full copyright information
//------------------------------------------------------------------------------

#ifndef _OGLCONSOLE_H
#define _OGLCONSOLE_H

#if defined(ZAP_DEDICATED) || defined(TNL_OS_MOBILE)
#  define BF_NO_CONSOLE
#endif


/* Opaque to you you lowly user */
struct _OGLCONSOLE_Console;
typedef _OGLCONSOLE_Console *OGLCONSOLE_Console;

#ifndef BF_NO_CONSOLE


#define MAX_CONSOLE_OUTPUT_LENGTH 4096


/* Initialize/uninitialize OGLConsole */
OGLCONSOLE_Console OGLCONSOLE_Create();
void OGLCONSOLE_Destroy(OGLCONSOLE_Console console);
void OGLCONSOLE_Quit();

void OGLCONSOLE_ShowConsole();
void OGLCONSOLE_HideConsole();

/* Set console which has PROGRAMMER focus (not application focus) */

/* This function renders the console */
void OGLCONSOLE_Draw();
void OGLCONSOLE_Render(OGLCONSOLE_Console console);

/* Set whether cursor should be visible -- can be used to make cursor blink */
void OGLCONSOLE_setCursor(int drawCursor);      

/* Handle resize window events */
void OGLCONSOLE_Reshape();

void OGLCONSOLE_Output(OGLCONSOLE_Console console, const char *s, ...);

/* Register a callback with the console */
void OGLCONSOLE_EnterKey(void(*cbfun)(OGLCONSOLE_Console console, char *cmd));

/* This function tries to handle the incoming keydown event. In the future there may
 * be non-SDL analogs for input systems such as GLUT. Returns true if the event
 * was handled by the console. If console is hidden, no events are handled. */
int OGLCONSOLE_KeyEvent(int key, int mod);
int OGLCONSOLE_CharEvent(int unicode);

/* Sets the dimensions of the console in lines and columns of characters. */
void OGLCONSOLE_SetDimensions(int width, int height);

/* Use this if you want to populate console command history yourself */
void OGLCONSOLE_AddHistory(OGLCONSOLE_Console console, char *s);

/* Show or hide the console */
void OGLCONSOLE_SetVisibility(int visible);
int OGLCONSOLE_GetVisibility();

/* Create the console font */
int OGLCONSOLE_CreateFont();

#endif // BF_NO_CONSOLE

#endif // _OGLCONSOLE_H

