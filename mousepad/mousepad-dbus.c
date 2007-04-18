/* $Id$ */
/*
 * Copyright (c) 2004-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2007      Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <dbus/dbus-glib-lowlevel.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus.h>

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-dbus.h>
#include <mousepad/mousepad-application.h>



#define MOUSEPAD_DBUS_PATH      "/org/xfce/Mousepad"
#define MOUSEPAD_DBUS_INTERFACE "org.xfce.Mousepad"



static void      mousepad_dbus_service_class_init    (MousepadDBusServiceClass  *klass);
static void      mousepad_dbus_service_init          (MousepadDBusService       *dbus_service);
static void      mousepad_dbus_service_finalize      (GObject                   *object);
static gboolean  mousepad_dbus_service_launch_files  (MousepadDBusService       *dbus_service,
                                                      const gchar               *working_directory,
                                                      gchar                    **filenames,
                                                      GError                   **error);



struct _MousepadDBusServiceClass
{
  GObjectClass __parent__;
};

struct _MousepadDBusService
{
  GObject __parent__;

  DBusGConnection *connection;
};



static GObjectClass *mousepad_dbus_service_parent_class;



GType
mousepad_dbus_service_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (G_TYPE_OBJECT,
                                            I_("MousepadDBusService"),
                                            sizeof (MousepadDBusServiceClass),
                                            (GClassInitFunc) mousepad_dbus_service_class_init,
                                            sizeof (MousepadDBusService),
                                            (GInstanceInitFunc) mousepad_dbus_service_init,
                                            0);
    }

  return type;
}



static void
mousepad_dbus_service_class_init (MousepadDBusServiceClass *klass)
{
  extern const DBusGObjectInfo  dbus_glib_mousepad_dbus_service_object_info;
  GObjectClass                 *gobject_class;

  /* determine the parent type class */
  mousepad_dbus_service_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = mousepad_dbus_service_finalize;

  /* install the D-BUS info for our class */
  dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass), &dbus_glib_mousepad_dbus_service_object_info);
}



static void
mousepad_dbus_service_init (MousepadDBusService *dbus_service)
{
  GError *error = NULL;

  /* try to connect to the session bus */
  dbus_service->connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

  if (G_LIKELY (dbus_service->connection != NULL))
    {
      /* register the /org/xfce/TextEditor object for Mousepad */
      dbus_g_connection_register_g_object (dbus_service->connection, MOUSEPAD_DBUS_PATH, G_OBJECT (dbus_service));

      /* request the org.xfce.Mousepad name for Mousepad */
      dbus_bus_request_name (dbus_g_connection_get_connection (dbus_service->connection),
                             MOUSEPAD_DBUS_INTERFACE, DBUS_NAME_FLAG_REPLACE_EXISTING, NULL);
    }
  else
    {
#ifdef NDEBUG
      /* hide this warning when the user is root and debug is disabled */
      if (geteuid () != 0)
#endif
        {
          /* notify the user that D-BUS service won't be available */
          g_message ("Failed to connect to the D-BUS session bus: %s\n", error->message);
        }

      g_error_free (error);
    }
}



static void
mousepad_dbus_service_finalize (GObject *object)
{
  MousepadDBusService *dbus_service = MOUSEPAD_DBUS_SERVICE (object);

  /* release the D-BUS connection object */
  if (G_LIKELY (dbus_service->connection != NULL))
    dbus_g_connection_unref (dbus_service->connection);

  (*G_OBJECT_CLASS (mousepad_dbus_service_parent_class)->finalize) (object);
}



/**
 * mousepad_dbus_service_launch_files:
 * @dbus_service      : A #MousepadDBusService.
 * @working_directory : The default working directory for this window.
 * @filenames         : A list of filenames we try to open in tabs. The file names
 *                      can either be absolute paths, supported URIs or relative file
 *                      names to @working_directory or %NULL for an untitled document.
 * @error             : Return location for errors, not used atm.
 *
 * This function is activated by DBus (service) and opens a new window in this instance of
 * Mousepad.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
static gboolean
mousepad_dbus_service_launch_files (MousepadDBusService  *dbus_service,
                                    const gchar          *working_directory,
                                    gchar               **filenames,
                                    GError              **error)
{
  MousepadApplication *application;
   GdkScreen          *screen;

  _mousepad_return_val_if_fail (g_path_is_absolute (working_directory), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* get the default screen */
  screen = gdk_screen_get_default ();

  /* open a mousepad window */
  application = mousepad_application_get ();
  mousepad_application_open_window (application, screen, working_directory, filenames);
  g_object_unref (G_OBJECT (application));

  return TRUE;
}



/**
 * mousepad_dbus_service_terminate:
 * @dbus_service : A #MousepadDBusService.
 * @error        : Return location for errors, not used atm.
 *
 * This function quits this instance of Mousepad.
 *
 * Return value: %TRUE on success.
 **/
static gboolean
mousepad_dbus_service_terminate (MousepadDBusService  *dbus_service,
                                 GError              **error)
{
  /* leave the Gtk main loop as soon as possible */
  gtk_main_quit ();

  /* we cannot fail */
  return TRUE;
}



/**
 * mousepad_dbus_client_send:
 * @message : A #DBusMessage.
 * @error   : Return location for errors or %NULL.
 *
 * This function sends the DBus message and should avoid
 * code duplication in the functions below.
 *
 * Return value: %TRUE on succeed or %FALSE if @error is set.
 **/
static gboolean
mousepad_dbus_client_send (DBusMessage  *message,
                           GError      **error)
{
  DBusConnection *connection;
  DBusMessage    *result;
  DBusError       derror;

  dbus_error_init (&derror);

  /* try to connect to the session bus */
  connection = dbus_bus_get (DBUS_BUS_SESSION, &derror);
  if (G_UNLIKELY (connection == NULL))
    {
      dbus_set_g_error (error, &derror);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* send the message */
  result = dbus_connection_send_with_reply_and_block (connection, message, -1, &derror);

  /* check if no reply was received */
  if (result == NULL)
    {
      /* check if there was just no instance running */
      if (!dbus_error_has_name (&derror, DBUS_ERROR_NAME_HAS_NO_OWNER))
        dbus_set_g_error (error, &derror);

      dbus_error_free (&derror);
      return FALSE;
    }

  /* but maybe we received an error */
  if (G_UNLIKELY (dbus_message_get_type (result) == DBUS_MESSAGE_TYPE_ERROR))
    {
      dbus_set_error_from_message (&derror, result);
      dbus_set_g_error (error, &derror);
      dbus_message_unref (result);
      dbus_error_free (&derror);
      return FALSE;
    }

  /* it seems everything worked */
  dbus_message_unref (result);

  return TRUE;
}



/**
 * mousepad_dbus_client_terminate:
 * @error : Return location for errors or %NULL.
 *
 * Function called from this instance of the application and tries to invoke
 * with an already running instance and ties to quit it.
 * The mousepad_dbus_service_terminate function is activated in the running instance.
 *
 * Return value: %TRUE on success.
 **/
gboolean
mousepad_dbus_client_terminate (GError **error)
{
  DBusMessage *message;

  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* generate the message */
  message = dbus_message_new_method_call (MOUSEPAD_DBUS_INTERFACE, MOUSEPAD_DBUS_PATH,
                                          MOUSEPAD_DBUS_INTERFACE, "Terminate");
  dbus_message_set_auto_start (message, FALSE);


  /* send the message */
  mousepad_dbus_client_send (message, error);

  /* unref the message */
  dbus_message_unref (message);

  /* we return false if an error was set */
  return (error != NULL);
}



/**
 * mousepad_dbus_client_launch_files:
 * @filenames         : A list of filenames we try to open in tabs. The file names
 *                      can either be absolute paths, supported URIs or relative file
 *                      names to @working_directory or %NULL for an untitled document.
 * @working_directory : Working directory for the new Mousepad window.
 * @error             : Return location for errors or %NULL.
 *
 * This function is called within this instance and tries to connect a running instance
 * of Mousepad via DBus. The function mousepad_dbus_service_launch_files is activated in the
 * running instance.
 *
 * Return value: %TRUE on success.
 **/
gboolean
mousepad_dbus_client_launch_files (gchar       **filenames,
                                   const gchar  *working_directory,
                                   GError      **error)
{
  DBusMessage *message;
  guint        length = 0;
  gboolean     succeed;

  _mousepad_return_val_if_fail (g_path_is_absolute (working_directory), FALSE);
  _mousepad_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* get the length of the filesname string */
  if (filenames)
    length = g_strv_length (filenames);

  /* generate the message */
  message = dbus_message_new_method_call (MOUSEPAD_DBUS_INTERFACE, MOUSEPAD_DBUS_PATH,
                                          MOUSEPAD_DBUS_INTERFACE, "LaunchFiles");
  dbus_message_set_auto_start (message, FALSE);
  dbus_message_append_args (message,
                            DBUS_TYPE_STRING, &working_directory,
                            DBUS_TYPE_ARRAY, DBUS_TYPE_STRING, &filenames, length,
                            DBUS_TYPE_INVALID);

  /* send the message */
  succeed = mousepad_dbus_client_send (message, error);

  /* unref the message */
  dbus_message_unref (message);

  return succeed;
}



/* include the dbus glue generated by dbus-binding-tool */
#include <mousepad/mousepad-dbus-infos.h>