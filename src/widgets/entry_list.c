#include "utils.h"
#include "entry.h"
#include "entry_listing.h"
#include "entry_list.h"

struct _EntryList
{
  GtkScrolledWindow parent_instance;

  GtkListBox *entry_list_box;
};

enum
{
  SIGNAL_SELECTION_CHANGED,
  LAST_SIGNAL,
};

static guint entry_list_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EntryList, entry_list, GTK_TYPE_SCROLLED_WINDOW);

static gint
sort_ptrarray_alphabetically(gconstpointer a, gconstpointer b)
{
    gchar *a_str = *(gchar **) a;
    gchar *b_str = *(gchar **) b;

    return -g_strcmp0 (a_str, b_str);
}

static GPtrArray *
list_entries (const gchar * dir_path)
{
  GDir *dir;
  GError *error;
  const gchar *filename;
  GPtrArray *files;
  files = g_ptr_array_new_full(50, g_free);

  dir = g_dir_open(dir_path, 0, &error);
  while ((filename = g_dir_read_name(dir))) {
    gchar *file_path = g_strdup_printf ("%s/%s", dir_path, filename);
    if (g_file_test (file_path, G_FILE_TEST_IS_REGULAR) &&
        g_str_has_suffix (file_path, ".md")) {
      g_ptr_array_add (files, g_strdup (filename));
    }
    g_free (file_path);
  }

  g_ptr_array_sort (files, (GCompareFunc) sort_ptrarray_alphabetically);
  g_dir_close (dir);

  return files;
}

static void
entry_pressed_cb (GtkListBox *entry_list_box, GtkListBoxRow *row, gpointer user_data)
{
  /* necessary for making no entry selected by default */
  gtk_list_box_set_selection_mode (entry_list_box, GTK_SELECTION_SINGLE);
  gtk_list_box_select_row (entry_list_box, row);
}

static void
entry_selected_changed_cb (GtkListBox *entry_list_box, gpointer user_data)
{
  EntryList *list = DIARY_ENTRY_LIST (user_data);
  Entry *entry = NULL;

  GtkListBoxRow *row = gtk_list_box_get_selected_row (entry_list_box);
  if (row) {
    GList *children;
    EntryListing *listing;

    children = gtk_container_get_children (GTK_CONTAINER (row));
    g_assert (children != NULL);
    listing = children->data;
    entry = entry_listing_get_entry (listing);
    g_list_free (children);
  }

  if (entry) {
    gchar *name;
    g_object_get (entry, "basename", &name, NULL);
    g_print ("%s\n", name);
  } else {
    g_print ("entry unselected\n");
  }

  g_signal_emit_by_name (list, "selection-changed", entry);
}

static GtkListBox *
generate_entry_list (EntryList *self, const gchar *dir_path, GPtrArray *files)
{
  const gchar *filename;
  GtkListBox *entry_list_box;

  entry_list_box = GTK_LIST_BOX (gtk_list_box_new ());
  gtk_list_box_set_selection_mode (entry_list_box, GTK_SELECTION_NONE);

  g_signal_connect (entry_list_box, "row-activated", G_CALLBACK (entry_pressed_cb), NULL);
  g_signal_connect (entry_list_box, "selected-rows-changed", G_CALLBACK (entry_selected_changed_cb), self);

  for (int i=0; i < files->len; i++) {
    filename = g_ptr_array_index (files, i);
    Entry *entry = entry_open (dir_path, filename);
    GtkWidget *entry_listing = GTK_WIDGET (entry_listing_new (entry));
    GtkWidget *row_widget = gtk_list_box_row_new ();
    gtk_container_add (GTK_CONTAINER (row_widget), entry_listing);
    gtk_list_box_insert (entry_list_box, row_widget, -1);
  }

  return entry_list_box;
}

static void
load_entry_list (EntryList *self, gpointer user_data)
{
  gchar *dir_path;
  GPtrArray *files;

  dir_path = utils_get_diary_folder ();
  files = list_entries (dir_path);
  self->entry_list_box = generate_entry_list (self, dir_path, files);
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->entry_list_box));
  gtk_widget_show_all (GTK_WIDGET (self));

  g_free (dir_path);
  g_ptr_array_unref (files);
}

static void
unload_entry_list (EntryList *self, gpointer user_data)
{
  gtk_container_remove (GTK_CONTAINER (self),
      GTK_WIDGET (self->entry_list_box));
  self->entry_list_box = NULL;
}

static void
entry_list_class_init (EntryListClass *klass)
{
  //GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  entry_list_signals [SIGNAL_SELECTION_CHANGED] =
      g_signal_new ("selection-changed", G_TYPE_FROM_CLASS (klass),
      0, 0, NULL, NULL, NULL, G_TYPE_NONE, 1, DIARY_TYPE_ENTRY);
}

static void
entry_list_init (EntryList *self)
{
  self->entry_list_box = NULL;
  // We load and unload on ever map/unmap so the entry list is updated in case
  // a entry has been deleted or added
  g_assert (g_signal_connect (self, "map", (GCallback) load_entry_list, NULL) > 0);
  g_assert (g_signal_connect (self, "unmap", (GCallback) unload_entry_list, NULL) > 0);

  gtk_widget_set_vexpand (GTK_WIDGET (self), TRUE);
  gtk_widget_set_hexpand (GTK_WIDGET (self), TRUE);

  gtk_widget_show_all (GTK_WIDGET (self));
}

GtkWidget *
entry_list_new (void)
{
    GtkWidget *entry_list = g_object_new (DIARY_TYPE_ENTRY_LIST, NULL);
    return entry_list;
}
