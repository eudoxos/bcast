#include <glib.h>

#ifdef G_OS_WIN32
static gboolean _g_main_poll_debug = TRUE;

#define STRICT
#include <windows.h>
#include <process.h>
#include <glib/gprintf.h>
#include <glib/giochannel.h>


#define TR1(a) { g_print("%d: ",__LINE__); g_print(a); g_print("\n"); }
#define TR2(a,b) { g_print("%d: ",__LINE__); g_print(a,b); g_print("\n"); }
#define TR3(a,b,c) { g_print("%d: ",__LINE__); g_print(a,b,c); g_print("\n"); }
#define TR4(a,b,c,d) { g_print("%d: ",__LINE__); g_print(a,b,c,d); g_print("\n"); }

static int
L_poll_rest (GPollFD *msg_fd,
           GPollFD *stop_fd,
           HANDLE  *handles,
           GPollFD *handle_to_fd[],
           gint     nhandles,
           gint     timeout_ms)
{
  DWORD ready;
  GPollFD *f;
  int recursed_result;

  if (msg_fd != NULL)
  {
      /* Wait for either messages or handles
       * -> Use MsgWaitForMultipleObjectsEx
       */
      TR1("waiting for messages OR handles");
      if (_g_main_poll_debug)
         g_print ("  MsgWaitForMultipleObjectsEx(%d, %d)\n", nhandles, timeout_ms);

      ready = MsgWaitForMultipleObjectsEx (nhandles, handles, timeout_ms,
             QS_ALLINPUT, MWMO_ALERTABLE);

      if (ready == WAIT_FAILED)
      {
         gchar *emsg = g_win32_error_message (GetLastError ());
         g_warning ("MsgWaitForMultipleObjectsEx failed: %s", emsg);
         g_free (emsg);
      }
   }
   else if (nhandles == 0)
   {
      /* No handles to wait for, just the timeout */
      if (timeout_ms == INFINITE)
         ready = WAIT_FAILED;
      else
        {
          /* Wait for the current process to die, more efficient than SleepEx(). */
          WaitForSingleObjectEx (GetCurrentProcess (), timeout_ms, TRUE);
          ready = WAIT_TIMEOUT;
        }
   }
   else
   {
      /* Wait for just handles
       * -> Use WaitForMultipleObjectsEx
       */
      TR1("waiting just for handles");
      if (_g_main_poll_debug)
         g_print ("  WaitForMultipleObjectsEx(%d, %d)\n", nhandles, timeout_ms);

      ready = WaitForMultipleObjectsEx (nhandles, handles, FALSE, timeout_ms, TRUE);
      if (ready == WAIT_FAILED)
      {
         gchar *emsg = g_win32_error_message (GetLastError ());
         g_warning ("WaitForMultipleObjectsEx failed: %s", emsg);
         g_free (emsg);
      }
   }

   if (_g_main_poll_debug)
      g_print ("  wait returns %ld%s\n",
        ready,
          (ready == WAIT_FAILED ? " (WAIT_FAILED)" :
            (ready == WAIT_TIMEOUT ? " (WAIT_TIMEOUT)" :
              (msg_fd != NULL && ready == WAIT_OBJECT_0 + nhandles ? " (msg)" : ""))));

   if (ready == WAIT_FAILED)
      return -1;
   else if (ready == WAIT_TIMEOUT ||
      ready == WAIT_IO_COMPLETION)
      return 0;
   else if (msg_fd != NULL && ready == WAIT_OBJECT_0 + nhandles)
      {
      msg_fd->revents |= G_IO_IN;

      /* If we have a timeout, or no handles to poll, be satisfied
       * with just noticing we have messages waiting.
       */
      if (timeout_ms != 0 || nhandles == 0)
          return 1;

      /* If no timeout and handles to poll, recurse to poll them,
       * too.
       */
      recursed_result = L_poll_rest (NULL, stop_fd, handles, handle_to_fd, nhandles, 0);
      return (recursed_result == -1) ? -1 : 1 + recursed_result;
    }
    else if (ready >= WAIT_OBJECT_0 && ready < WAIT_OBJECT_0 + nhandles)
    {
      int retval;

      f = handle_to_fd[ready - WAIT_OBJECT_0];
      f->revents = f->events;
      if (_g_main_poll_debug)
        g_print ("  got event %p\n", (HANDLE) f->fd);

      /* Do not count the stop_fd */
      retval = (f != stop_fd) ? 1 : 0;

      /* If no timeout and polling several handles, recurse to poll
       * the rest of them.
       */
      if (timeout_ms == 0 && nhandles > 1)
        {
          /* Poll the handles with index > ready */
          HANDLE *shorter_handles;
          GPollFD **shorter_handle_to_fd;
          gint shorter_nhandles;

          shorter_handles = &handles[ready - WAIT_OBJECT_0 + 1];
          shorter_handle_to_fd = &handle_to_fd[ready - WAIT_OBJECT_0 + 1];
          shorter_nhandles = nhandles - (ready - WAIT_OBJECT_0 + 1);

          recursed_result = L_poll_rest (NULL, stop_fd, shorter_handles, shorter_handle_to_fd, shorter_nhandles, 0);
          return (recursed_result == -1) ? -1 : retval + recursed_result;
        }
      return retval;
    }

  return 0;
}

typedef struct
{
  HANDLE handles[MAXIMUM_WAIT_OBJECTS];
  GPollFD *handle_to_fd[MAXIMUM_WAIT_OBJECTS];
  GPollFD *msg_fd;
  GPollFD *stop_fd;
  gint nhandles;
  gint timeout_ms;
} GWin32PollThreadData;

static gint
L_poll_single_thread (GWin32PollThreadData *data)
{
  int retval;

  /* Polling for several things? */
  if (data->nhandles > 1 || (data->nhandles > 0 && data->msg_fd != NULL))
    {
      /* First check if one or several of them are immediately
       * available
       */
      retval = L_poll_rest (data->msg_fd, data->stop_fd, data->handles, data->handle_to_fd, data->nhandles, 0);

      /* If not, and we have a significant timeout, poll again with
       * timeout then. Note that this will return indication for only
       * one event, or only for messages.
       */
      if (retval == 0 && (data->timeout_ms == INFINITE || data->timeout_ms > 0))
        retval = L_poll_rest (data->msg_fd, data->stop_fd, data->handles, data->handle_to_fd, data->nhandles, data->timeout_ms);
    }
  else
    {
      TR1("polling one object only");
      /* Just polling for one thing, so no need to check first if
       * available immediately
       */
      retval = L_poll_rest (data->msg_fd, data->stop_fd, data->handles, data->handle_to_fd, data->nhandles, data->timeout_ms);
    }

  return retval;
}

static void
L_fill_poll_thread_data (GPollFD              *fds,
                       guint                 nfds,
                       gint                  timeout_ms,
                       GPollFD              *stop_fd,
                       GWin32PollThreadData *data)
{
  GPollFD *f;

  data->timeout_ms = timeout_ms;

  if (stop_fd != NULL)
    {
      if (_g_main_poll_debug)
        g_print (" Stop FD: %p", (HANDLE) stop_fd->fd);

      g_assert (data->nhandles < MAXIMUM_WAIT_OBJECTS);

      data->stop_fd = stop_fd;
      data->handle_to_fd[data->nhandles] = stop_fd;
      data->handles[data->nhandles++] = (HANDLE) stop_fd->fd;
    }

  for (f = fds; f < &fds[nfds]; ++f)
    {
      if ((data->nhandles == MAXIMUM_WAIT_OBJECTS) ||
          (data->msg_fd != NULL && (data->nhandles == MAXIMUM_WAIT_OBJECTS - 1)))
        {
          g_warning ("Too many handles to wait for!");
          break;
        }

      if (f->fd == G_WIN32_MSG_HANDLE && (f->events & G_IO_IN))
        {
          if (_g_main_poll_debug && data->msg_fd == NULL)
            g_print (" MSG");
          data->msg_fd = f;
        }
      else if (f->fd > 0)
        {
          if (_g_main_poll_debug)
            g_print (" %p", (HANDLE) f->fd);
          data->handle_to_fd[data->nhandles] = f;
          data->handles[data->nhandles++] = (HANDLE) f->fd;
        }

      f->revents = 0;
    }
}

static guint __stdcall
L_poll_thread_run (gpointer user_data)
{
  GWin32PollThreadData *data = user_data;

  /* Docs say that it is safer to call _endthreadex by our own:
   * https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/endthread-endthreadex
   */
  _endthreadex (L_poll_single_thread (data));

  g_assert_not_reached ();

  return 0;
}

/* One slot for a possible msg object or the stop event */
#define MAXIMUM_WAIT_OBJECTS_PER_THREAD (MAXIMUM_WAIT_OBJECTS - 1)

gint
L_g_poll (GPollFD *fds,
  guint    nfds,
  gint     timeout)
{
  guint nthreads, threads_remain;
  HANDLE thread_handles[MAXIMUM_WAIT_OBJECTS];
  GWin32PollThreadData *threads_data;
  GPollFD stop_event = { 0, };
  GPollFD *f;
  guint i, fds_idx = 0;
  DWORD ready;
  DWORD thread_retval;
  int retval;
  GPollFD *msg_fd = NULL;

  g_print("g_poll: USING LOCAL VERSION\n");

  if (timeout == -1)
    timeout = INFINITE;

  /* Simple case without extra threads */
  if (nfds <= MAXIMUM_WAIT_OBJECTS)
    {
      GWin32PollThreadData data = { 0, };

      if (_g_main_poll_debug)
        g_print ("g_poll: waiting for");

      L_fill_poll_thread_data (fds, nfds, timeout, NULL, &data);

      if (_g_main_poll_debug)
        g_print ("\n");

      retval = L_poll_single_thread (&data);
      if (retval == -1)
        for (f = fds; f < &fds[nfds]; ++f)
          f->revents = 0;

      return retval;
    }

  if (_g_main_poll_debug)
    g_print ("g_poll: polling with threads\n");

  nthreads = nfds / MAXIMUM_WAIT_OBJECTS_PER_THREAD;
  threads_remain = nfds % MAXIMUM_WAIT_OBJECTS_PER_THREAD;
  if (threads_remain > 0)
    nthreads++;

  if (nthreads > MAXIMUM_WAIT_OBJECTS_PER_THREAD)
    {
      g_warning ("Too many handles to wait for in threads!");
      nthreads = MAXIMUM_WAIT_OBJECTS_PER_THREAD;
    }

#if GLIB_SIZEOF_VOID_P == 8
  stop_event.fd = (gint64)CreateEventW (NULL, TRUE, FALSE, NULL);
#else
  stop_event.fd = (gint)CreateEventW (NULL, TRUE, FALSE, NULL);
#endif
  stop_event.events = G_IO_IN;

  threads_data = g_new0 (GWin32PollThreadData, nthreads);
  for (i = 0; i < nthreads; i++)
    {
      guint thread_fds;
      guint ignore;

      if (i == (nthreads - 1) && threads_remain > 0)
        thread_fds = threads_remain;
      else
        thread_fds = MAXIMUM_WAIT_OBJECTS_PER_THREAD;

      L_fill_poll_thread_data (fds + fds_idx, thread_fds, timeout, &stop_event, &threads_data[i]);
      fds_idx += thread_fds;

      /* We must poll for messages from the same thread, so poll it along with the threads */
      if (threads_data[i].msg_fd != NULL)
        {
          msg_fd = threads_data[i].msg_fd;
          threads_data[i].msg_fd = NULL;
        }

      thread_handles[i] = (HANDLE) _beginthreadex (NULL, 0, L_poll_thread_run, &threads_data[i], 0, &ignore);
    }

  /* Wait for at least one thread to return */
  if (msg_fd != NULL)
    ready = MsgWaitForMultipleObjectsEx (nthreads, thread_handles, timeout,
                                         QS_ALLINPUT, MWMO_ALERTABLE);
  else
    ready = WaitForMultipleObjects (nthreads, thread_handles, FALSE, timeout);

  /* Signal the stop in case any of the threads did not stop yet */
  if (!SetEvent ((HANDLE)stop_event.fd))
    {
      gchar *emsg = g_win32_error_message (GetLastError ());
      g_warning ("gpoll: failed to signal the stop event: %s", emsg);
      g_free (emsg);
    }

  /* Wait for the rest of the threads to finish */
  WaitForMultipleObjects (nthreads, thread_handles, TRUE, INFINITE);

  /* The return value of all the threads give us all the fds that changed state */
  retval = 0;
  if (msg_fd != NULL && ready == WAIT_OBJECT_0 + nthreads)
    {
      msg_fd->revents |= G_IO_IN;
      retval = 1;
    }

  for (i = 0; i < nthreads; i++)
    {
      if (GetExitCodeThread (thread_handles[i], &thread_retval))
        retval = retval == -1 ? -1 : thread_retval == -1 ? -1 : retval + thread_retval;

      CloseHandle (thread_handles[i]);
    }

  if (retval == -1)
    for (f = fds; f < &fds[nfds]; ++f)
      f->revents = 0;

  g_free (threads_data);
  CloseHandle ((HANDLE)stop_event.fd);

  return retval;
}



#endif



