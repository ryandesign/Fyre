/*
 * explorer-animation.c - Implements the explorer GUI's animation editor
 *
 * de Jong Explorer - interactive exploration of the Peter de Jong attractor
 * Copyright (C) 2004 David Trowbridge and Micah Dowty
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "explorer.h"
#include "curve-editor.h"
#include "cell-renderer-transition.h"
#include <stdlib.h>
#include <math.h>

static void explorer_update_animation_length(Explorer *self);
static void explorer_init_keyframe_view(Explorer *self);

static void on_anim_play_toggled(GtkWidget *widget, gpointer user_data);
static void on_keyframe_add(GtkWidget *widget, gpointer user_data);
static void on_keyframe_replace(GtkWidget *widget, gpointer user_data);
static void on_keyframe_delete(GtkWidget *widget, gpointer user_data);
static void on_keyframe_view_cursor_changed(GtkWidget *widget, gpointer user_data);
static gboolean on_anim_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static void on_anim_new(GtkWidget *widget, gpointer user_data);
static void on_anim_open(GtkWidget *widget, gpointer user_data);
static void on_anim_save(GtkWidget *widget, gpointer user_data);
static void on_anim_save_as(GtkWidget *widget, gpointer user_data);
static void on_anim_scale_changed(GtkWidget *widget, gpointer user_data);
static void on_anim_set_linear(GtkWidget *widget, gpointer user_data);
static void on_anim_set_smooth(GtkWidget *widget, gpointer user_data);
static void on_anim_curve_changed(GtkWidget *widget, gpointer user_data);
static void on_keyframe_duration_change(GtkWidget *widget, gpointer user_data);


/************************************************************************************/
/**************************************************** Initialization / Finalization */
/************************************************************************************/

void explorer_init_animation(Explorer *self) {

  /* Connect signal handlers
   */
  glade_xml_signal_connect_data(self->xml, "on_anim_play_toggled",            G_CALLBACK(on_anim_play_toggled),            self);
  glade_xml_signal_connect_data(self->xml, "on_keyframe_add",                 G_CALLBACK(on_keyframe_add),                 self);
  glade_xml_signal_connect_data(self->xml, "on_keyframe_replace",             G_CALLBACK(on_keyframe_replace),             self);
  glade_xml_signal_connect_data(self->xml, "on_keyframe_delete",              G_CALLBACK(on_keyframe_delete),              self);
  glade_xml_signal_connect_data(self->xml, "on_keyframe_view_cursor_changed", G_CALLBACK(on_keyframe_view_cursor_changed), self);
  glade_xml_signal_connect_data(self->xml, "on_anim_window_delete",           G_CALLBACK(on_anim_window_delete),           self);
  glade_xml_signal_connect_data(self->xml, "on_anim_new",                     G_CALLBACK(on_anim_new),                     self);
  glade_xml_signal_connect_data(self->xml, "on_anim_open",                    G_CALLBACK(on_anim_open),                    self);
  glade_xml_signal_connect_data(self->xml, "on_anim_save",                    G_CALLBACK(on_anim_save),                    self);
  glade_xml_signal_connect_data(self->xml, "on_anim_save_as",                 G_CALLBACK(on_anim_save_as),                 self);
  glade_xml_signal_connect_data(self->xml, "on_anim_scale_changed",           G_CALLBACK(on_anim_scale_changed),           self);
  glade_xml_signal_connect_data(self->xml, "on_anim_set_linear",              G_CALLBACK(on_anim_set_linear),              self);
  glade_xml_signal_connect_data(self->xml, "on_anim_set_smooth",              G_CALLBACK(on_anim_set_smooth),              self);
  glade_xml_signal_connect_data(self->xml, "on_keyframe_duration_change",     G_CALLBACK(on_keyframe_duration_change),     self);

  /* Add our CurveEditor, a modified GtkCurve widget
   */
  self->anim_curve = curve_editor_new();
  gtk_container_add(GTK_CONTAINER(glade_xml_get_widget(self->xml, "anim_curve_box")), self->anim_curve);
  g_signal_connect(self->anim_curve, "changed", G_CALLBACK(on_anim_curve_changed), self);
  gtk_widget_show_all(self->anim_curve);

  explorer_update_animation_length(self);
  explorer_init_keyframe_view(self);
}

void explorer_dispose_animation(Explorer *self) {
  if (self->animation) {
    g_object_unref(self->animation);
    self->animation = NULL;
  }
}


/************************************************************************************/
/****************************************************************** Keyframe Editor */
/************************************************************************************/

static void explorer_init_keyframe_view(Explorer *self) {
  GtkTreeView *tv = GTK_TREE_VIEW(glade_xml_get_widget(self->xml, "keyframe_view"));
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;

  gtk_tree_view_set_model(tv, GTK_TREE_MODEL(self->animation->model));

  /* The first column only displays the keyframe, in the form of a thumbnail
   */
  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(col, "Keyframe");

  renderer = gtk_cell_renderer_pixbuf_new();
  gtk_tree_view_column_pack_start(col, renderer, FALSE);
  gtk_tree_view_column_set_attributes(col, renderer,
				      "pixbuf", ANIMATION_MODEL_THUMBNAIL,
				      NULL);

  gtk_tree_view_append_column(tv, col);

  /* The second column uses a custom cell renderer to show the curve and duration
   */
  col = gtk_tree_view_column_new();
  gtk_tree_view_column_set_title(col, "Transition");

  renderer = cell_renderer_transition_new();
  gtk_tree_view_column_pack_start(col, renderer, FALSE);
  gtk_tree_view_column_set_attributes(col, renderer,
				      "spline", ANIMATION_MODEL_SPLINE,
				      "duration", ANIMATION_MODEL_DURATION,
				      NULL);

  gtk_tree_view_append_column(tv, col);

}

static void explorer_get_current_keyframe(Explorer *self, GtkTreeIter *iter) {
  GtkTreePath *path;
  GtkTreeView *tv = GTK_TREE_VIEW(glade_xml_get_widget(self->xml, "keyframe_view"));
  gtk_tree_view_get_cursor(tv, &path, NULL);
  gtk_tree_model_get_iter(GTK_TREE_MODEL(self->animation->model), iter, path);
  gtk_tree_path_free(path);
}

static void on_keyframe_add(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  animation_keyframe_append(self->animation, self->dejong);
  explorer_update_animation_length(self);
}

static void on_keyframe_replace(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GtkTreeIter iter;
  explorer_get_current_keyframe(self, &iter);
  animation_keyframe_store_dejong(self->animation, &iter, self->dejong);
}

static void on_keyframe_delete(GtkWidget *widget, gpointer user_data) {
  /* Determine which row the cursor is on, delete it, and make the delete
   * button insensitive again until another row is selected.
   */
  Explorer *self = EXPLORER(user_data);
  GtkTreeIter iter;
  explorer_get_current_keyframe(self, &iter);

  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "keyframe_delete_button"), FALSE);
  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "keyframe_replace_button"), FALSE);
  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "anim_transition_box"), FALSE);

  gtk_list_store_remove(self->animation->model, &iter);
  explorer_update_animation_length(self);
}

static void on_keyframe_view_cursor_changed(GtkWidget *widget, gpointer user_data) {
  /* This is called when a new row in the keyframe view is selected.
   * enable the delete button, and load this row's parameters.
   */
  Explorer *self = EXPLORER(user_data);
  GtkTreeIter iter;
  Spline *spline;
  gdouble keyframe_duration;

  explorer_get_current_keyframe(self, &iter);

  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "keyframe_delete_button"), TRUE);
  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "keyframe_replace_button"), TRUE);
  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "anim_transition_box"), TRUE);

  if (!self->seeking_animation) {
    /* Assuming the user clicked us rather than this being called as the result of
     * a seek, seek the animation to this keyframe's location.
     */
    self->selecting_keyframe = TRUE;
    gtk_range_set_value(GTK_RANGE(glade_xml_get_widget(self->xml, "anim_scale")),
			animation_keyframe_get_time(self->animation, &iter));

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(self->xml, "anim_play_button")), FALSE);
    self->selecting_keyframe = FALSE;
  }

  /* Load this keyframe's transition parameters into our GUI */
  self->allow_transition_changes = FALSE;
  gtk_tree_model_get(GTK_TREE_MODEL(self->animation->model), &iter,
		     ANIMATION_MODEL_DURATION, &keyframe_duration,
		     ANIMATION_MODEL_SPLINE,   &spline,
		     -1);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(glade_xml_get_widget(self->xml, "keyframe_duration")), keyframe_duration);
  curve_editor_set_spline(CURVE_EDITOR(self->anim_curve), spline);
  spline_free(spline);
  self->allow_transition_changes = TRUE;
}



static void on_anim_set_linear(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  curve_editor_set_spline(CURVE_EDITOR(self->anim_curve), &spline_template_linear);
}

static void on_anim_set_smooth(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  curve_editor_set_spline(CURVE_EDITOR(self->anim_curve), &spline_template_smooth);
}

static void on_keyframe_duration_change(GtkWidget *widget, gpointer user_data) {
  /* The user just changed the current keyframe's duration.
   * Update the tree model, and recalculate the size of our animation.
   */
  Explorer *self = EXPLORER(user_data);
  GtkTreeIter iter;

  if (!self->allow_transition_changes)
    return;

  explorer_get_current_keyframe(self, &iter);

  gtk_list_store_set(self->animation->model, &iter,
		     ANIMATION_MODEL_DURATION, (gdouble) gtk_spin_button_get_value(GTK_SPIN_BUTTON(widget)),
		     -1);

  explorer_update_animation_length(self);
}

static void on_anim_curve_changed(GtkWidget *widget, gpointer user_data) {
  /* The user just changed the current keyframe's spline, update the model
   */
  Explorer *self = EXPLORER(user_data);
  GtkTreeIter iter;
  Spline *spline;

  if (!self->allow_transition_changes)
    return;

  explorer_get_current_keyframe(self, &iter);

  spline = curve_editor_get_spline(CURVE_EDITOR(self->anim_curve));
  gtk_list_store_set(self->animation->model, &iter,
		     ANIMATION_MODEL_SPLINE, spline,
		     -1);
  spline_free(spline);
}


/************************************************************************************/
/****************************************************************** Playing/seeking */
/************************************************************************************/

void explorer_update_animation(Explorer *self) {
  /* Move on to the next frame if we're playing an animation
   */
  GTimeVal now;
  double diff, new_value;
  GtkRange *range;
  GtkAdjustment *adj;
  GtkWidget *loop_widget;

  if (!self->playing_animation)
    return;

  g_get_current_time(&now);
  diff = ((now.tv_usec - self->last_anim_frame_time.tv_usec) / 1000000.0 +
	  (now.tv_sec  - self->last_anim_frame_time.tv_sec ));
  self->last_anim_frame_time = now;

  range = GTK_RANGE(glade_xml_get_widget(self->xml, "anim_scale"));
  adj = gtk_range_get_adjustment(range);

  new_value = adj->value + diff;

  if (new_value < adj->upper) {
    gtk_range_set_value(range, new_value);
  }
  else {
    /* We've reached the end... */

    loop_widget = glade_xml_get_widget(self->xml, "loop_animation");
    if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(loop_widget))) {
      /* Loop the animation */
      gtk_range_set_value(range, adj->lower);
    }
    else {
      /* Stop at the end */
      gtk_range_set_value(range, adj->upper);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(self->xml, "anim_play_button")), FALSE);
    }
  }
}

static void on_anim_play_toggled(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GtkRange *range = GTK_RANGE(glade_xml_get_widget(self->xml, "anim_scale"));

  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {

    /* If our animation is already at its end, start it over */
    if (gtk_range_get_value(range) >= gtk_range_get_adjustment(range)->upper - 0.1)
      gtk_range_set_value(range, 0);

    g_get_current_time(&self->last_anim_frame_time);
    self->playing_animation = TRUE;
  }
  else {
    self->playing_animation = FALSE;
  }
}

static void explorer_update_animation_length(Explorer *self) {
  /* Recalculate the length of the animation and update the anim_scale accordingly
   */
  GtkWidget *scale = glade_xml_get_widget(self->xml, "anim_scale");
  gdouble length = animation_get_length(self->animation);
  gboolean enable = length > 0.0001;

  /* To keep the GtkRange from complaining */
  if (!enable)
    length = 1;

  gtk_range_set_adjustment(GTK_RANGE(scale),
			   GTK_ADJUSTMENT(gtk_adjustment_new(gtk_range_get_value(GTK_RANGE(scale)), 0, length, 0.01, 1, 0)));
  gtk_widget_set_sensitive(scale, enable);
  gtk_widget_set_sensitive(glade_xml_get_widget(self->xml, "anim_play_button"), enable);
}

static void on_anim_scale_changed(GtkWidget *widget, gpointer user_data) {
  double v = gtk_range_get_adjustment(GTK_RANGE(widget))->value;
  Explorer *self = EXPLORER(user_data);
  AnimationIter iter;
  GtkTreePath *path;
  GtkTreeView *tv = GTK_TREE_VIEW(glade_xml_get_widget(self->xml, "keyframe_view"));

  /* Seek to the right place in the animation and load an interpolated frame */
  animation_iter_seek(self->animation, &iter, v);
  if (!iter.valid) {
    /* Don't do anything if we're past the end of the animation.
     * This will happen because the animation's range is only [0, length)
     * while the scale widget we represent it has a range of [0, length]
     */
    return;
  }
  animation_iter_load_dejong(self->animation, &iter, self->dejong);

  if (!self->selecting_keyframe) {
    /* Put the tree view's cursor on the current keyframe */
    path = gtk_tree_model_get_path(GTK_TREE_MODEL(self->animation->model), &iter.keyframe);
    self->seeking_animation = TRUE;
    gtk_tree_view_set_cursor(tv, path, NULL, FALSE);
    self->seeking_animation = FALSE;
    gtk_tree_path_free(path);
  }

  if (!self->playing_animation) {
    /* Just like the color picker, the hscale will probably try to suck up
     * all of the idle time we might have been spending rendering things.
     * Force at least a little rendering to happen right now.
     */
    explorer_run_iterations(self);
    explorer_update_gui(self);
  }
}


/************************************************************************************/
/******************************************************************** Menu Commands */
/************************************************************************************/

static gboolean on_anim_window_delete(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  /* Just hide the window when the user tries to close it
   */
  Explorer *self = EXPLORER(user_data);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(glade_xml_get_widget(self->xml, "toggle_animation_window")), FALSE);
  return TRUE;
}

static void on_anim_new(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  animation_clear(self->animation);
  explorer_update_animation_length(self);
}

static void on_anim_open(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GtkWidget *dialog;

  dialog = gtk_file_selection_new("Open Animation Keyframes");

  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
    const gchar *filename;
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    animation_load_file(self->animation, filename);
    explorer_update_animation_length(self);
  }
  gtk_widget_destroy(dialog);
}

static void on_anim_save(GtkWidget *widget, gpointer user_data) {
  on_anim_save_as(widget, user_data);
}

static void on_anim_save_as(GtkWidget *widget, gpointer user_data) {
  Explorer *self = EXPLORER(user_data);
  GtkWidget *dialog;

  dialog = gtk_file_selection_new("Save Animation Keyframes");
  gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog), "animation.dja");

  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
    const gchar *filename;
    filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(dialog));
    animation_save_file(self->animation, filename);
  }
  gtk_widget_destroy(dialog);
}

/* The End */