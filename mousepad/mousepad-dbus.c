/*
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

#include <mousepad/mousepad-private.h>
#include <mousepad/mousepad-dbus.h>
/* include the dbus glue generated by gdbus-codegen */
#include <mousepad/mousepad-dbus-infos.h>
#include <mousepad/mousepad-application.h>

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif



#define MOUSEPAD_DBUS_PATH      "/org/xfce/Mousepad"
#define MOUSEPAD_DBUS_INTERFACE "org.xfce.Mousepad"



static gboolean  mousepad_dbus_service_launch_files  (MousepadDBusService    *dbus_service,
                                                      GDBusMethodInvocation  *invocation,
                                                      const gchar            *working_directory,
                                                      gchar                 **filenames,
                                                      gpointer                user_data);
static gboolean  mousepad_dbus_service_terminate     (MousepadDBusService   *dbus_service,
                                                      GDBusMethodInvocation *invocation,
                                                      gpointer               user_data);



static void
on_name_acquired (GDBusConnection *connection,
                  const gchar *name,
                  gpointer user_data)
{
  MousepadDBusService *skeleton;

  skeleton = mousepad_dbus_service_skeleton_new ();
  g_signal_connect (skeleton,
                    "handle-launch-files",
                    G_CALLBACK (mousepad_dbus_service_launch_files),
                    NULL);
  g_signal_connect (skeleton,
                    "handle-terminate",
                    G_CALLBACK (mousepad_dbus_service_terminate),
                    NULL);
  g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton),
                                    connection,
                                    MOUSEPAD_DBUS_PATH,
                                    NULL);
}



void
mousepad_dbus_service_init (void)
{
  g_bus_own_name (G_BUS_TYPE_SESSION,
                  MOUSEPAD_DBUS_INTERFACE,
                  G_BUS_NAME_OWNER_FLAGS_REPLACE,
                  NULL,
                  on_name_acquired,
                  NULL,
                  NULL,
                  NULL);
}



/**
 * mousepad_dbus_service_launch_files:
 * @dbus_service      : A #MousepadDBusService.
 * @invocation        : A #GDBusMethodInvocation
 * @working_directory : The default working directory for this window.
 * @filenames         : A list of filenames we try to open in tabs. The file names
 *                      can either be absolute paths, supported URIs or relative file
 *                      names to @working_directory or %NULL for an untitled document.
 * @user_data         : User data, not used atm.
 *
 * This function is activated by DBus (service) and opens a new window in this instance of
 * Mousepad.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
static gboolean
mousepad_dbus_service_launch_files (MousepadDBusService    *dbus_service,
                                    GDBusMethodInvocation  *invocation,
                                    const gchar            *working_directory,
                                    gchar                 **filenames,
                                    gpointer                user_data)
{
  MousepadApplication *application;

  if (!g_path_is_absolute (working_directory))
    {
      g_dbus_method_invocation_return_error_literal (invocation,
                                                     G_DBUS_ERROR,
                                                     G_DBUS_ERROR_INVALID_ARGS,
                                                     "Argument working_directory must be an absolute path");
      return FALSE;
    }

  /* open a mousepad window */
  application = mousepad_application_get ();
  mousepad_application_new_window_with_files (application, NULL, working_directory, filenames);
  g_object_unref (G_OBJECT (application));

  mousepad_dbus_service_complete_launch_files (dbus_service, invocation);

  return TRUE;
}



/**
 * mousepad_dbus_service_terminate:
 * @dbus_service : A #MousepadDBusService.
 * @invocation   : A #GDBusMethodInvocation
 * @error        : User data, not used atm.
 *
 * This function quits this instance of Mousepad.
 *
 * Return value: %TRUE on success.
 **/
static gboolean
mousepad_dbus_service_terminate (MousepadDBusService   *dbus_service,
                                 GDBusMethodInvocation *invocation,
                                 gpointer               user_data)
{
  /* leave the Gtk main loop as soon as possible */
  gtk_main_quit ();

  mousepad_dbus_service_complete_terminate (dbus_service, invocation);

  /* we cannot fail */
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
  gboolean             succeed;
  MousepadDBusService *proxy;

  proxy = mousepad_dbus_service_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        MOUSEPAD_DBUS_INTERFACE,
                                                        MOUSEPAD_DBUS_PATH,
                                                        NULL, error);

  g_return_val_if_fail (proxy != NULL, FALSE);

  succeed = mousepad_dbus_service_call_terminate_sync (proxy, NULL, error);

  g_object_unref (proxy);

  return succeed;
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
  MousepadDBusService *proxy;
  guint                length = 0;
  gboolean             succeed = FALSE;
  GPtrArray           *utf8_filenames;
  gchar               *utf8_dir = NULL;

  g_return_val_if_fail (g_path_is_absolute (working_directory), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  utf8_filenames = g_ptr_array_new_with_free_func(g_free);
  if (filenames != NULL)
    {
      guint i;
      /* get the length of the filesname string */
      length = g_strv_length (filenames);
      g_ptr_array_set_size(utf8_filenames, length + 1);
      /* encode locale filenames to UTF-8 for DBus */
      for (i = 0; i < length; i++)
        {
          gchar *utf8_fn = g_filename_to_utf8(filenames[i], -1, NULL, NULL, error);
          if (utf8_fn == NULL)
            {
              g_ptr_array_free(utf8_filenames, TRUE);
              return FALSE;
            }
          utf8_filenames->pdata[i] = utf8_fn;
        }
    }
  g_ptr_array_add(utf8_filenames, NULL);

  if (working_directory != NULL)
    {
      /* encode working directory to UTF-8 for DBus */
      utf8_dir = g_filename_to_utf8(working_directory, -1, NULL, NULL, error);
      if (utf8_dir == NULL)
        {
          g_ptr_array_free(utf8_filenames, TRUE);
          return FALSE;
        }
    }

  proxy = mousepad_dbus_service_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        MOUSEPAD_DBUS_INTERFACE,
                                                        MOUSEPAD_DBUS_PATH,
                                                        NULL, error);

  if (proxy)
    {
     succeed = mousepad_dbus_service_call_launch_files_sync (
        proxy, utf8_dir, utf8_filenames->pdata, NULL, error);

      g_object_unref (proxy);
    }

  /* cleanup the UTF-8 strings */
  g_ptr_array_free(utf8_filenames, TRUE);
  g_free(utf8_dir);

  return succeed;
}
