#include <stdio.h>
#include <string.h>
#include <gst/gst.h>

static GMainLoop *loop;

static gboolean
my_bus_callback (GstBus     *bus,
                 GstMessage *message,
                 gpointer    data)
{
    //g_print ("Got %s message\n", GST_MESSAGE_TYPE_NAME (message));

    switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR: {
        GError *err;
        gchar *debug;

        gst_message_parse_error (message, &err, &debug);
        g_print ("Error: %s\n", err->message);
        g_error_free (err);
        g_free (debug);

        g_main_loop_quit (loop);
        break;
    }
    case GST_MESSAGE_EOS:
        /* end-of-stream */
        g_main_loop_quit (loop);
        break;
    default:
        /* unhandled message */
        break;
    }

    return TRUE;
}

gint
main (int   argc,
      char *argv[])
{
    /* process command-line arguments */
    gchar *device = NULL;
    GOptionContext *ctx;
    GError *err = NULL;

    //TODO Add option with argument for
    ctx = g_option_context_new ("Device camera live player");
    g_option_context_add_group (ctx, gst_init_get_option_group ());
    if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
        g_print ("Failed to initialize: %s\n", err->message);
        g_clear_error (&err);
        g_option_context_free (ctx);
        return 1;
    }

    /* initialize gst */
    gst_init(0, NULL);

    /* create sink element */
    GstElement *sink;
    sink = NULL;

    /* use glimagesink as default */
    sink = gst_element_factory_make ("glimagesink", "sink");
    if (!sink) {
        /* try autovideosink as fallback*/
        sink = gst_element_factory_make ("autovideosink", "sink");
        if (!sink) {
            g_error ("Failed to create element of type 'glimagesink' and "
                     "fallback 'autovideosink' also failed\n");
            g_option_context_free (ctx);
            return -1;
        }
    }


    /* create source element */
    GstElement *src;
    src = gst_element_factory_make ("v4l2src", "source");
    if (!src) {
        g_error ("Failed to create element of type 'v4l2src'\n");
        gst_object_unref (GST_OBJECT (sink));
        g_option_context_free (ctx);
        return -1;
    }
    /* use device argument for source */
    if (device) {
        g_object_set (G_OBJECT (src), "device", device, NULL);
    }

    /*Fixup resolution and scale to screen size*/
    GstElement *vscale;
    GstElement *cfilter;
    vscale = gst_element_factory_make ("videoscale", "videoscale");
    cfilter = gst_element_factory_make ("capsfilter", "capsfilter");

    gchar scaling[34] = "video/x-raw,width=1280,height=720"; //Frame size for seilfe camera 1600x1200
                                                             //Frame size for front camera 1280x720

    /*Flip video iface*/
//TODO add sensor config
    GstElement *videoflip;
    videoflip = gst_element_factory_make ("videoflip", "videoflip");
    if (!videoflip)
    {
        g_error("Cannot create videoflip element");
        return -1;
    }
    /*
none (0) – Identity (no rotation)
clockwise (1) – Rotate clockwise 90 degrees
rotate-180 (2) – Rotate 180 degrees
counterclockwise (3) – Rotate counter-clockwise 90 degrees
horizontal-flip (4) – Flip horizontally
vertical-flip (5) – Flip vertically
upper-left-diagonal (6) – Flip across upper left/lower right diagonal
upper-right-diagonal (7) – Flip across upper right/lower left diagonal
automatic (8) – Select flip method based on image-orientation tag
    */
    g_object_set (G_OBJECT(videoflip), "method", 7, NULL);

    /* create pipeline */
    GstElement *pipeline;
    pipeline = gst_pipeline_new ("ppcam");

    GstCaps *caps = gst_caps_from_string (scaling);
    g_object_set (G_OBJECT (cfilter), "caps", caps, NULL);

    gst_bin_add_many (GST_BIN (pipeline), src, vscale, cfilter, videoflip, sink, NULL);
    if (!gst_element_link_many (src, vscale, cfilter, videoflip, sink, NULL)) {
      g_error ("Failed to link elements");
      gst_object_unref (GST_OBJECT (pipeline)); // unrefs also its elements
      g_option_context_free (ctx);
      return -1;
    }

    /* add watch to pipeline message bus */
    GstBus *bus;
    guint bus_watch_id;
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    bus_watch_id = gst_bus_add_watch (bus, my_bus_callback, NULL);
    gst_object_unref (bus);

    /* play */
    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    /* main loop */
    loop = g_main_loop_new (NULL, FALSE);
    g_main_loop_run (loop);

    /* clean up */
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (pipeline)); // unrefs also its elements
    g_source_remove (bus_watch_id);
    g_main_loop_unref (loop);
    g_option_context_free (ctx);

    return 0;
}


