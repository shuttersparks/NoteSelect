#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

//Reference below if needed.
//MessageBox(GTK_WINDOW(gtk_widget_get_toplevel(w)),"Test");

GtkWidget *mainwindow;
GtkWidget *thevbox; //the vbox that contains menu and scrolled fixedfield
GtkWidget *scrolled_window;
GtkWidget *layout;
GtkAdjustment *hadj, *vadj;

//Notes are managed as a linked list of the following structure
typedef struct note NOTE;
NOTE *notes;

struct note {
  GtkWidget *textcont;
  GtkWidget *textview;
  NOTE *fwdptr;
  NOTE *backptr;
};

NOTE *currfocusnote; //current top note, that has highlighted color
char *currfilename; //points to a buffer containing the current filename or a null string if none
gboolean dirty_flag; //dirty flag for note movement.  does not indicate changes to text.
gboolean search_dialog_active; //true if dialog active. (prevents more than one search dialog active)

//Working values for dragging notes around the layout area
int layoffx, layoffy; //offset of UL corner of layout area on screen
int ffwid, ffhgt; //width and height of fixedfield
int noteoffx, noteoffy; //offset of widget within fixedfield
int notewid, notehgt; //width and height of note
int imoffx, imoffy; //initial offset of mouse within text widget at moment of button click
int dragx, dragy; //note offset used during movement
int scrollposh, scrollposv; //scrollbar position

gboolean resize_mode; //true if in note resize mode

//Color Values
GdkColor act_fg_color, act_bg_color;
GdkColor inact_fg_color, inact_bg_color;
GdkColor field_bg_color;



void set_appwindow_title(char *s) {
  gchar buf[200];

  g_snprintf( buf, 199, "Note Select - %s", s);
  gtk_window_set_title(GTK_WINDOW(mainwindow), buf);
}

void MessageBox(GtkWindow *parentWindow, char *msg) {
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (parentWindow, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, msg);
  gtk_window_set_title (GTK_WINDOW (dialog), "Information");
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

static void execute_search( GtkEntryBuffer *eb, guint position, guint n_chars, gpointer user_data) {
  NOTE *noteptr;
  GtkTextBuffer *gtktextbuf;
  GtkTextIter iterstart, iterend;
  const gchar *entrybuffer;  //do not attempt to free this pointer
  gchar *foldedentrystring, *notestring, *foldednotestring;

  entrybuffer = gtk_entry_buffer_get_text(eb);  //do not attempt to free this pointer
  foldedentrystring = g_utf8_casefold( entrybuffer, -1 );
  noteptr = notes;
  while( noteptr ) {
    gtktextbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(noteptr->textview));
    gtk_text_buffer_get_bounds( gtktextbuf, &iterstart, &iterend );
    notestring = gtk_text_buffer_get_text( gtktextbuf, &iterstart, &iterend, FALSE);
    foldednotestring = g_utf8_casefold( notestring, -1 );
    if( g_strstr_len(foldednotestring, -1, foldedentrystring) ) {
      gtk_widget_show( noteptr->textcont );  //match
      }
    else {
      gtk_widget_hide( noteptr->textcont );  //no match
      }
    g_free(notestring);
    g_free(foldednotestring);
    noteptr = noteptr->fwdptr;
    }

  g_free(foldedentrystring);
}

void destroy_search_dialog(GtkWidget *widget) {
  gtk_widget_show_all(layout);
  gtk_widget_destroy(widget);
  search_dialog_active = FALSE;
}

void execute_search_dialog(void) {
  GtkWidget *dialog, *label, *content_area, *entry;
  GtkEntryBuffer *entrybuffer;       

  if( search_dialog_active ) return;
  search_dialog_active = TRUE;
  //Create the widgets
  dialog = gtk_dialog_new_with_buttons ("Find",
                                         GTK_WINDOW(mainwindow),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_CLOSE,
                                         GTK_RESPONSE_NONE,
                                         NULL);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  label = gtk_label_new("Search text:");
  //Ensure that the dialog box is destroyed when the user responds.
  g_signal_connect_swapped (dialog,
                            "response",
                            G_CALLBACK(destroy_search_dialog),
                            dialog);
  //create single line text entry field
  entry = gtk_entry_new();
  gtk_entry_set_max_length( GTK_ENTRY(entry), 25);
  gtk_entry_set_width_chars( GTK_ENTRY(entry), 25);
  entrybuffer = gtk_entry_get_buffer( GTK_ENTRY(entry) );

  //Add the content area items, and show everything
  gtk_container_add(GTK_CONTAINER(content_area), label);
  gtk_container_add(GTK_CONTAINER(content_area), entry);

  g_signal_connect( entrybuffer, "inserted-text", GTK_SIGNAL_FUNC(execute_search), NULL);
  g_signal_connect( entrybuffer, "deleted-text", GTK_SIGNAL_FUNC(execute_search), NULL);

  gtk_widget_show_all(dialog);
}


void destroy_note(NOTE *noteptr) {
  if( noteptr ) {
    if( noteptr == currfocusnote ) currfocusnote = NULL;
    gtk_widget_destroy( noteptr->textview );
    gtk_widget_destroy( noteptr->textcont );

    //unlink this node
    if( noteptr->backptr ) {
      noteptr->backptr->fwdptr = noteptr->fwdptr;
      }
    if( noteptr->fwdptr ) {
      noteptr->fwdptr->backptr = noteptr->backptr;
      }
    //if this node's backptr is NULL we're at beginning of list. set root pointer = fwdptr.
    if( noteptr->backptr == NULL ) {
      notes = noteptr->fwdptr;  //(if this is NULL, this node is the last one so notes will become NULL.)
      }
    g_free( noteptr );
    }
}

void destroy_all_notes() {
  NOTE *noteptr;

  noteptr = notes;
  while( noteptr ) {
    destroy_note( noteptr );
  noteptr = notes; //(destroying a note modifies the linked list so we start at the root each time)
  }
}

static void resize_the_layout() {
  NOTE *ptr;
  int xmax, ymax;
  int nx, ny; //offset of origin of note within layout area
  int nwid, nhgt; //width and height of note

  ptr = notes;  //base of linked list
  xmax = 0;  ymax = 0;
  while( ptr != NULL ) {
    gdk_window_get_position(ptr->textview->window, &nx, &ny);  //get note origin position
    // width and height of note
    nwid = ptr->textcont->allocation.width;
    nhgt = ptr->textcont->allocation.height;
//printf("%i, %i   %i, %i\n", nx, ny, nwid, nhgt ); //for debug

    if( (nx+nwid) > xmax ) xmax = nx+nwid;
    if( (ny+nhgt) > ymax ) ymax = ny+nhgt;

    ptr = ptr->fwdptr;
    }

  //a little extra space:
  xmax += 20; ymax += 20;
//printf("New layout size: %i, %i\n", xmax, ymax );
  gtk_layout_set_size( GTK_LAYOUT(layout), xmax, ymax );
  return;
}

//button released and ctrl key is pressed.  end of drag. resize the layout.
static gboolean button_release_event(GtkWidget *w, GdkEventButton *event) {
  if( (event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK ) resize_the_layout();
  return FALSE;
}

static gboolean button_press_event(GtkWidget *w, GdkEventButton *event) {
  NOTE *noteptr;

  resize_mode = FALSE;
  noteptr = g_object_get_data( G_OBJECT(w), "noteptr");
  gdk_window_raise(gtk_widget_get_window(GTK_WIDGET(w))); //Don't use widget_get_window until window is realized!

  if( currfocusnote != NULL && currfocusnote->textview != w ) {
    gtk_widget_modify_text( currfocusnote->textview, GTK_STATE_NORMAL, &inact_fg_color);
    gtk_widget_modify_base( currfocusnote->textview, GTK_STATE_NORMAL, &inact_bg_color);
    }

  gtk_widget_modify_text( noteptr->textview, GTK_STATE_NORMAL, &act_fg_color);
  gtk_widget_modify_base( noteptr->textview, GTK_STATE_NORMAL, &act_bg_color);
  currfocusnote = noteptr;

  if( (event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK ) {
    //At the moment of button click, capture the positions and sizes of the fixedfield, the note,
    //and the mouse.
    //event->x,y is offset of mouse within the note
    imoffx = (int)event->x;
    imoffy = (int)event->y;

		// noteoffx,y is offset of note within the layout area
		gdk_window_get_position(noteptr->textview->window, &noteoffx, &noteoffy);

    // width and height of note
    notewid = noteptr->textview->allocation.width;
    notehgt = noteptr->textview->allocation.height;

    //offset of layout area on the screen
		gdk_window_get_position(scrolled_window->window, &layoffx, &layoffy);
    layoffy += 35;  //fudge factor

    // width and height of layout area
    ffwid = layout->allocation.width;
    ffhgt = layout->allocation.height;

//printf("BP wpos %i, %i; eventxy %i, %i; wid,hgt %i, %i;\n", noteoffx, noteoffy, imoffx, imoffy, notewid, notehgt);
//printf(" layoffx,y %i, %i; ffwid,hgt %i, %i", layoffx, layoffy, ffwid, ffhgt);
//printf("\n");
    return TRUE;
    }
	return FALSE;
}

static gboolean motion_notify_event( GtkWidget *widget, GdkEventMotion *event ) {
  int x, y;

  if( currfocusnote == NULL ) return FALSE; //error, should not happen
  if( (event->state & (GDK_CONTROL_MASK | GDK_BUTTON1_MASK)) == (GDK_CONTROL_MASK | GDK_BUTTON1_MASK))
    {
    x = (int)event->x_root; //coords of mouse pointer on screen
    y = (int)event->y_root;

    hadj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
    vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(scrolled_window));
    scrollposh = (int)gtk_adjustment_get_value(hadj);
    scrollposv = (int)gtk_adjustment_get_value(vadj);

    //compute new offset for note within layout area
    dragx = ((x - imoffx) - layoffx) + scrollposh;
    dragy = ((y - imoffy) - layoffy) + scrollposv;

    //test if user is dragging out of bounds
    if( dragx < 1 ) return TRUE;
    if( dragy < 1 ) return TRUE;

    gtk_layout_move(GTK_LAYOUT(layout), currfocusnote->textcont, dragx, dragy);
    dirty_flag = TRUE;
	  }
	return TRUE;
}

void create_note( char *t, int x, int y, int xs, int ys ) {
  GtkTextBuffer *gtktextbufptr;
  NOTE *ptr, *newptr;

  ptr = notes;
  newptr = g_malloc( sizeof(NOTE) );
  if( ptr != NULL ) {
    //faster to insert at the head of the list
    newptr->fwdptr = notes;
    notes->backptr = newptr;
    }
  else {
    newptr->fwdptr = NULL;
    }
  newptr->backptr = NULL;
  notes = newptr;

  newptr->textcont = gtk_fixed_new();
  g_object_set_data( G_OBJECT(newptr->textcont), "noteptr", newptr );
  gtk_widget_set_size_request(newptr->textcont, xs, ys);
  gtk_layout_put(GTK_LAYOUT(layout), newptr->textcont, x, y);

  // create multiline text widget
  newptr->textview = gtk_text_view_new();
  g_object_set_data( G_OBJECT(newptr->textview), "noteptr", newptr );
  gtk_container_add(GTK_CONTAINER(newptr->textcont), newptr->textview);

	gtk_signal_connect(GTK_OBJECT(newptr->textcont), "motion_notify_event", (GtkSignalFunc)motion_notify_event, NULL);

	gtk_widget_add_events(newptr->textview, GDK_BUTTON1_MASK | GDK_BUTTON_RELEASE_MASK);

	gtk_signal_connect(GTK_OBJECT(newptr->textview), "button_press_event", (GtkSignalFunc)button_press_event, NULL);
	gtk_signal_connect(GTK_OBJECT(newptr->textview), "button_release_event", (GtkSignalFunc)button_release_event, NULL);

  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(newptr->textview),GTK_WRAP_WORD_CHAR);
  gtk_widget_set_size_request(newptr->textview, xs, ys);
  gtk_widget_modify_text(newptr->textview, GTK_STATE_NORMAL, &inact_fg_color);
  gtk_widget_modify_base(newptr->textview, GTK_STATE_NORMAL, &inact_bg_color);

  //get ptr to text buffer and set the starting text
  gtktextbufptr = gtk_text_view_get_buffer(GTK_TEXT_VIEW(newptr->textview));
  gtk_text_buffer_set_text(gtktextbufptr, t, -1);
  gtk_text_buffer_set_modified(gtktextbufptr, FALSE);  //clear the dirty flag

  return;
}

gboolean is_stack_dirty() {
  NOTE *noteptr;
  GtkTextBuffer *gtktextbufptr;

  noteptr = notes;
  while( noteptr ) {
    gtktextbufptr = gtk_text_view_get_buffer(GTK_TEXT_VIEW(noteptr->textview));
    if( gtk_text_buffer_get_modified(gtktextbufptr) ) return(TRUE);
    noteptr = noteptr->fwdptr;
    }
  return(FALSE);
}

void clear_all_dirty_flags() {
  NOTE *noteptr;
  GtkTextBuffer *gtktextbufptr;

  noteptr = notes;
  while( noteptr ) {
    gtktextbufptr = gtk_text_view_get_buffer(GTK_TEXT_VIEW(noteptr->textview));
    gtk_text_buffer_set_modified(gtktextbufptr, FALSE);
    noteptr = noteptr->fwdptr;
    }
}

//Local function for compression and to keep note to file writing consistent.
static void write_note_to_file(NOTE *noteptr, FILE *fp) {
  GtkTextBuffer *gtktextbuf;
  GtkTextIter iterstart, iterend;
  gchar *bufferstring;
  int buflen; //byte length of text buffer
  int xpos, ypos, nw, nh; //note position on layout x,y, width, height

 	gdk_window_get_position(noteptr->textview->window, &xpos, &ypos);
  nw = noteptr->textcont->allocation.width;
  nh = noteptr->textcont->allocation.height;

  gtktextbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(noteptr->textview));
  gtk_text_buffer_get_bounds( gtktextbuf, &iterstart, &iterend );
  bufferstring = gtk_text_buffer_get_text( gtktextbuf, &iterstart, &iterend, FALSE);
  buflen = strlen(bufferstring); // byte length of UTF-8 string including NUL
  fprintf(fp,"%06i%06i%06i%06i%06i", xpos, ypos, nw, nh, buflen+1);
  fwrite(bufferstring, sizeof(char), buflen, fp);
  fputc( 0, fp); //write a zero
}

static void save_file(char *filename) {
  FILE *fp;
  NOTE *noteptr;

  if( filename ) {
    fp = fopen( filename, "w");
    if( fp == NULL ) return;
    noteptr = notes;
    while( noteptr ) {
      write_note_to_file( noteptr, fp);
      noteptr = noteptr->fwdptr;
      }
    fprintf(fp,"000000000000000000000000000000");
    fclose(fp);
    }
  printf("\n");
}

static void save_selected_to_file(char *filename) {
  FILE *fp;
  NOTE *noteptr;

  if( filename ) {
    fp = fopen( filename, "w");
    if( fp == NULL ) return;
    noteptr = notes;
    while( noteptr ) {
      if( gtk_widget_get_visible( noteptr->textcont )) {
        write_note_to_file( noteptr, fp);
        }
      noteptr = noteptr->fwdptr;
      }
    fprintf(fp,"000000000000000000000000000000");
    fclose(fp);
    }
  printf("\n");
}

static void export_file_as_text(char *filename) {
  FILE *fp;
  NOTE *noteptr;
  GtkTextBuffer *gtktextbuf;
  GtkTextIter iterstart, iterend;
  gchar *bufferstring;
  int buflen; //byte length of text buffer

  if( filename ) {
    fp = fopen( filename, "w");
    if( fp == NULL ) return;
    noteptr = notes;
    while( noteptr ) {
      gtktextbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(noteptr->textview));
      gtk_text_buffer_get_bounds( gtktextbuf, &iterstart, &iterend );
      bufferstring = gtk_text_buffer_get_text( gtktextbuf, &iterstart, &iterend, FALSE);
      buflen = strlen(bufferstring); // byte length of UTF-8 string including NUL
      fwrite(bufferstring, sizeof(char), buflen, fp);
      fprintf(fp,"\n$@$@$@\n");

      noteptr = noteptr->fwdptr;
      }
    fclose(fp);
    }
}

static void load_file(char *filename) {
  FILE *fp;
  gchar *buffer;
  int buflen; //byte length of text buffer
  int xpos, ypos, nw, nh, textlen; //note position on layout x,y, width, height

  if( filename ) {
    fp = fopen( filename, "r");
    if( fp == NULL ) return;

    buffer = g_malloc(1000);  //start at 1000.  will increase if necessary
    buflen = 1000;

    while(1) {
      if( fscanf(fp,"%6d%6d%6d%6d%6d", &xpos, &ypos, &nw, &nh, &textlen) != 5 ) {
        printf("Error reading file at scanf\n");
        break;
        }
      if( nw==0 && nh==0 ) break;  //end of file
      if( textlen+1 >= buflen ) {
        g_free(buffer);
        buflen = textlen + 1000;
        buffer = g_malloc(buflen);
        }
      if( fread( buffer, sizeof(char), textlen, fp) != textlen ) {
        printf("Error reading file at fread\n");
        break;
        }

      create_note( buffer, xpos, ypos, nw, nh);
      if( feof(fp) ) break;
      }
    g_free(buffer);
    }
  fclose(fp);
}

gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
  int nw, nh;

  switch( event->keyval ) {
    case GDK_Escape:
      printf("Escape key pressed\n");
      break;
    case GDK_Right:
    case GDK_Left:
    case GDK_Up:
    case GDK_Down:
      if( resize_mode && currfocusnote ) {
        // current width and height of note
        nw = currfocusnote->textcont->allocation.width;
        nh = currfocusnote->textcont->allocation.height;
        switch( event->keyval ) {
          case GDK_Right: nw += 10; break;
          case GDK_Left: nw -= 10; break;
          case GDK_Up: nh -= 10; break;
          case GDK_Down: nh += 10; break;
          default: break;
          }
        nw = (nw / 10) * 10;
        nh = (nh / 10) * 10;
        if( nw < 50 ) nw = 50;
        if( nh < 20 ) nh = 20;
        gtk_widget_set_size_request( currfocusnote->textcont, nw, nh);
        gtk_widget_set_size_request( currfocusnote->textview, nw, nh);
        dirty_flag = TRUE;
        }
      break;
    default:
      resize_mode = FALSE;
      return FALSE; 
    }
  return FALSE; 
}

static char *open_file_dialog() {
  GtkWidget *dialog;
  char *filename = NULL;
  gchar *path;
     
  dialog = gtk_file_chooser_dialog_new ("Select Existing File",
			GTK_WINDOW(mainwindow),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);

  if( currfilename[0] != 0 ) {
    path = g_path_get_dirname(currfilename);
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), path);
    g_free(path);
    }
     
  if( gtk_dialog_run(GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT ) {
    filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog) );
    }
 
  gtk_widget_destroy(dialog);
  return(filename);
}

// legend = string such as "Save As..." or "New Stack"
static char *save_file_dialog(char *legend) {
  GtkWidget *dialog;
  char *filename = NULL;
  gchar *path;
     
  dialog = gtk_file_chooser_dialog_new (legend,
			GTK_WINDOW(mainwindow),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);

  if( currfilename[0] != 0 ) {
    path = g_path_get_dirname(currfilename);
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), path);
    g_free(path);
    }
     
  if( gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT ) {
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER (dialog));
    }
 
  gtk_widget_destroy(dialog);
  return(filename);
}

gboolean check_for_save() {
  GtkWidget *dialog;
  gboolean retval = FALSE;

  if( is_stack_dirty() || dirty_flag ) {
    //we need to prompt for save
    dialog = gtk_message_dialog_new( NULL, 
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_QUESTION,
                GTK_BUTTONS_YES_NO,
                "Save changes?");
    gtk_window_set_title( GTK_WINDOW (dialog), "Save?");
    if( gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_NO)
      retval = FALSE;
    else
      retval = TRUE;
    gtk_widget_destroy( dialog );      
    }     
  return retval;
}

static void menuexecute_saveas(void) {
  char *filename;

  filename = save_file_dialog("Save Stack As...");
  if( filename != NULL ) {
    strncpy( currfilename, filename, 199);
    save_file(filename);
    set_appwindow_title(filename);
    clear_all_dirty_flags();
    dirty_flag = FALSE;
    }
}

static void menuexecute_save(void) {
  if( currfilename[0] != 0 ) {
    save_file(currfilename);
    clear_all_dirty_flags();
    dirty_flag = FALSE;
    }
  else {
    menuexecute_saveas();
    }
}

static void menuexecute_quit(void) {
  if( check_for_save() ) {
    menuexecute_save();  
    }
  gtk_main_quit();
}

gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer *data) {
  if( check_for_save() ) {
    menuexecute_save();  
    }
  return FALSE;  //pass event onward
}

static void menuexecute_new(void) {
  if( check_for_save() ) {
    menuexecute_save();  
    }
  destroy_all_notes();
  set_appwindow_title("Unnamed File");
  currfilename[0] = 0;
  create_note( "New note", 200, 200, 150, 150 );
  clear_all_dirty_flags();
  dirty_flag = FALSE;
  gtk_widget_show_all(layout);
}

static void menuexecute_open(void) {
  char *filename;

  filename = open_file_dialog();
  if( filename ) {
    destroy_all_notes();
    strncpy( currfilename, filename, 199);
    load_file(filename);
    gtk_widget_show_all( layout );
    set_appwindow_title(filename);
    clear_all_dirty_flags();
    dirty_flag = FALSE;
    }
}

//Save selected notes as a new stack file but don't do anything else.  Continue editing the same stack as before.
static void menuexecute_saveselectedas(void) {
  char *filename;

  filename = save_file_dialog("Save Selected Notes As New Stack...");
  if( filename != NULL ) {
    save_selected_to_file(filename);
    }
}

static void menuexecute_exportastext(void) {
  char *filename;

  filename = save_file_dialog("Export Stack As Text...");
  if( filename != NULL ) {
    export_file_as_text(filename);
    }
}

//Import a stack to the current stack.  Continue editing the current stack.
static void menuexecute_import(void) {
  char *filename;

  filename = open_file_dialog();
  if( filename != NULL ) {
    load_file(filename);
    gtk_widget_show_all( layout );
    dirty_flag = TRUE;
    }
}

static void menuexecute_note(void) {
  create_note( "New note", 200, 100, 300, 60 );
  dirty_flag = TRUE;
  gtk_widget_show_all( layout );
}

static void menuexecute_delete(void) {
  if( currfocusnote ) {
    destroy_note( currfocusnote );
    currfocusnote = NULL;
    dirty_flag = TRUE;
    }
}

static void menuexecute_resize(void) {
  if( currfocusnote != NULL ) resize_mode = TRUE;
}

static void menuexecute_repack(void) {
  printf("Repack Menu\n");
}

static void menuexecute_find(void) {
  execute_search_dialog();
}

int main( int argc, char *argv[])
{
  //Menu Item Declarations
  GtkWidget *menubar;
  //File menu (has submenu)
  GtkWidget *filemenu;
  GtkWidget *menuitem_file;
  GtkWidget *menuitem_new;
  GtkWidget *menuitem_open;
  GtkWidget *menuitem_save;
  GtkWidget *menuitem_saveas;
  GtkWidget *menuitem_saveselectedas;
  GtkWidget *menuitem_exportastext;
  GtkWidget *menuitem_import;
  GtkWidget *menuitem_quit;

  //Other top level menu items
  GtkWidget *menuitem_note;
  GtkWidget *menuitem_delete;
  GtkWidget *menuitem_resize;
  GtkWidget *menuitem_repack;
  GtkWidget *menuitem_find;

  gtk_init(&argc, &argv);

  notes = NULL;  //linked list is empty

  mainwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  set_appwindow_title("No File");
  gtk_window_set_default_size(GTK_WINDOW(mainwindow), 900, 600);
  gtk_window_set_position(GTK_WINDOW(mainwindow), GTK_WIN_POS_CENTER);

  //Create and build menus
  thevbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(mainwindow), thevbox);

  menubar = gtk_menu_bar_new();
  filemenu = gtk_menu_new();

  //File menu with submenus
  menuitem_file = gtk_menu_item_new_with_label("File");
  menuitem_new = gtk_menu_item_new_with_label("New");
  menuitem_open = gtk_menu_item_new_with_label("Open");
  menuitem_save = gtk_menu_item_new_with_label("Save");
  menuitem_saveas = gtk_menu_item_new_with_label("Save As");
  menuitem_saveselectedas = gtk_menu_item_new_with_label("Save Selected As");
  menuitem_exportastext = gtk_menu_item_new_with_label("Export As Text");
  menuitem_import = gtk_menu_item_new_with_label("Import");
  menuitem_quit = gtk_menu_item_new_with_label("Quit");

  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menuitem_file), filemenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), menuitem_new);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), menuitem_open);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), menuitem_save);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), menuitem_saveas);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), menuitem_saveselectedas);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), menuitem_exportastext);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), menuitem_import);
  gtk_menu_shell_append(GTK_MENU_SHELL(filemenu), menuitem_quit);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitem_file);

  menuitem_note = gtk_menu_item_new_with_label("Note");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitem_note);

  menuitem_delete = gtk_menu_item_new_with_label("Delete");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitem_delete);

  menuitem_resize = gtk_menu_item_new_with_label("Resize");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitem_resize);

  menuitem_repack = gtk_menu_item_new_with_label("Repack");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitem_repack);

  menuitem_find = gtk_menu_item_new_with_label("Find");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), menuitem_find);

  gtk_box_pack_start(GTK_BOX(thevbox), menubar, FALSE, FALSE, 3);

  //Attach menu handlers
  gtk_signal_connect_object( GTK_OBJECT(menuitem_quit), "activate", GTK_SIGNAL_FUNC(menuexecute_quit), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_new), "activate", GTK_SIGNAL_FUNC(menuexecute_new), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_open), "activate", GTK_SIGNAL_FUNC(menuexecute_open), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_save), "activate", GTK_SIGNAL_FUNC(menuexecute_save), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_saveas), "activate", GTK_SIGNAL_FUNC(menuexecute_saveas), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_saveselectedas), "activate", GTK_SIGNAL_FUNC(menuexecute_saveselectedas), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_exportastext), "activate", GTK_SIGNAL_FUNC(menuexecute_exportastext), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_import), "activate", GTK_SIGNAL_FUNC(menuexecute_import), NULL);

  //These menu items are top level and have no submenu, so we have to use the "button-press-event" signal
  gtk_signal_connect_object( GTK_OBJECT(menuitem_note), "button-press-event", GTK_SIGNAL_FUNC(menuexecute_note), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_delete), "button-press-event", GTK_SIGNAL_FUNC(menuexecute_delete), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_resize), "button-press-event", GTK_SIGNAL_FUNC(menuexecute_resize), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_repack), "button-press-event", GTK_SIGNAL_FUNC(menuexecute_repack), NULL);
  gtk_signal_connect_object( GTK_OBJECT(menuitem_find), "button-press-event", GTK_SIGNAL_FUNC(menuexecute_find), NULL);

  //force colors here.  later get from config.
  gdk_color_parse ("#ffff00", &act_bg_color);
  gdk_color_parse ("#000000", &act_fg_color);
  gdk_color_parse ("#e0e000", &inact_bg_color);
  gdk_color_parse ("#000000", &inact_fg_color);
  gdk_color_parse ("#505050", &field_bg_color);

  //Create a layout container that will contain all the notes.  The layout is contained in
  //a scrolled window container.  The scrolled window container is contained in a vbox.
  layout = gtk_layout_new( NULL, NULL );  //create layout container
  scrolled_window = gtk_scrolled_window_new( NULL, NULL ); //create scrolled window container
  gtk_box_pack_start( GTK_BOX(thevbox), scrolled_window, TRUE, TRUE, 3); //put scrolled window into vbox
  gtk_container_add( GTK_CONTAINER(scrolled_window), layout);  //put layout into scrolled window
  gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(scrolled_window),
                                  GTK_POLICY_AUTOMATIC, //horiz policy
                                  GTK_POLICY_ALWAYS); //vertical policy
  gtk_widget_modify_bg( layout, GTK_STATE_NORMAL, &field_bg_color);

//============

  create_note( "A note.", 200, 200, 150, 150 );

  currfocusnote = NULL;
  resize_mode = FALSE;
  dirty_flag = FALSE;
  search_dialog_active = FALSE;
  currfilename = g_malloc(200);
  currfilename[0] = 0;  //zero length string

  g_signal_connect( G_OBJECT(mainwindow), "delete_event", G_CALLBACK(on_window_delete), NULL);
  g_signal_connect_swapped( G_OBJECT(mainwindow), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect( G_OBJECT(mainwindow), "key_press_event", G_CALLBACK (on_key_press), NULL);

  gtk_widget_show_all(mainwindow);

  gtk_main();

  return 0;
}
