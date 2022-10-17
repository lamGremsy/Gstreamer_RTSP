#include <gst/gst.h>
#include <gst/app/app.h>
#include <gst/rtsp-server/rtsp-server.h>
#include <gst/base/gstbasetransform.h>
#include <gst/video/gstvideofilter.h>
#include <gst/video/video-format.h>
#include <gst/video/video-info.h>
#include <gst/video/video.h>

#include <cairo.h>
#include <cairo-gobject.h>
#include <glib.h>

#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>

static GstElement *pipeline , *tee;
static GstElement *queue_640, *cairo_overlay_640, *videoconvert_640, *videoscale_640, *filter_640, *x264enc_640, *appsink_640;
static GstElement *queue_320, *cairo_overlay_320, *videoconvert_320, *videoscale_320, *filter_320, *x264enc_320, *appsink_320;
static GstElement *queue_record, *videoconvert_record, *x264enc_record, *mp4mux_record, *filesink_record;
static GstElement *queue_capture, *videoconvert_capture, *jpegenc_capture, *filesink_capture;
static GstElement *videoconvert_640_c, *videoconvert_320_c;
static GstCaps    *caps_640 , *caps_320;

static char folder_record[100]  ="../Videos";
static char folder_capture[100] ="../Picture";
static char *file_record  = (char*) malloc(100*sizeof(char));
static char *file_capture = (char*) malloc(100*sizeof(char));
static char *ip = (char*) malloc(100*sizeof(char));
static gboolean recording = false;
static gint stt_record, stt_capture;

typedef struct
{
  gboolean valid;
  GstVideoInfo vinfo;
  char *resolution = (char*) malloc(100*sizeof(char));
} CairoOverlayState;

typedef struct
{
  GstElement *generator_pipe;
  GstElement *vid_appsink;
  GstElement *vid_appsrc;
} MyContext;

char* 
check_IP(){
    int stt;
    std::string data;
    FILE * stream;
    const int max_buffer = 256;
    char buffer[max_buffer];
    std::string cmd = "hostname -I";
    char *check = (char*) malloc(100*sizeof(char));
    stream = popen(cmd.c_str(), "r");
    char *result = (char*) malloc(100*sizeof(char));
    if (stream) {
    while (!feof(stream))
        if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
    pclose(stream);
    }
    // vector<string> buf = split(data, ' ');
    strcpy(check, data.c_str());
    if (strlen(check)<15)return (char*)"127.0.0.1";
    for (stt=0; stt<strlen(check);stt++){
        if (check[stt]==(char)*" "){
            memcpy(result, check, stt);
            return result;
        }
    }
    return (char*)"127.0.0.1";;
}

static void
prepare_overlay (GstElement * overlay, GstCaps * caps, gpointer user_data)
{
    CairoOverlayState *state = (CairoOverlayState *)user_data;

    // gst_video_format_parse_caps (caps, NULL, &state->width, &state->height);
    state->valid = gst_video_info_from_caps (&state->vinfo, caps);
    state->valid = TRUE;
}

static void
draw_overlay (GstElement * overlay, cairo_t * cr, guint64 timestamp,
            guint64 duration, gpointer user_data)
{
    CairoOverlayState *s = (CairoOverlayState *)user_data;
    int width, height;
    double scale;
    char *text = (char*) malloc(100*sizeof(char));
    ip = check_IP();
    if (!s->valid)
        return;

    width = GST_VIDEO_INFO_WIDTH (&s->vinfo);
    height = GST_VIDEO_INFO_HEIGHT (&s->vinfo);
    sprintf(text, "IP: %s",ip);

    scale = 2*(((timestamp/(int)1e7) % 70)+30)/100.0;
    cairo_translate(cr, 0, 0);
    cairo_move_to (cr, 20, 30);
    cairo_set_font_size(cr, 20);
    cairo_set_source_rgba (cr, 1, 0, 0, 1);
    cairo_show_text(cr, text);
    sprintf(text, "Resolution\\: %s",s->resolution);
    cairo_move_to (cr, 20, 50);
    cairo_set_font_size(cr, 20);
    cairo_set_source_rgba (cr, 1, 0, 0, 1);
    cairo_show_text(cr, text);
    cairo_stroke (cr);
}

static void
need_data (GstElement * appsrc, guint unused, MyContext * ctx)
{
  GstSample *sample;
  GstFlowReturn ret;

  if (appsrc == ctx->vid_appsrc)
    sample = gst_app_sink_pull_sample (GST_APP_SINK (ctx->vid_appsink));

  if (sample) {
    GstBuffer *buffer = gst_sample_get_buffer (sample);
    GstSegment *seg = gst_sample_get_segment (sample);
    GstClockTime pts, dts;

    /* Convert the PTS/DTS to running time so they start from 0 */
    pts = GST_BUFFER_PTS (buffer);
    if (GST_CLOCK_TIME_IS_VALID (pts))
      pts = gst_segment_to_running_time (seg, GST_FORMAT_TIME, pts);

    dts = GST_BUFFER_DTS (buffer);
    if (GST_CLOCK_TIME_IS_VALID (dts))
      dts = gst_segment_to_running_time (seg, GST_FORMAT_TIME, dts);

    if (buffer) {
      buffer = gst_buffer_copy (buffer);
      GST_BUFFER_PTS (buffer) = pts;
      GST_BUFFER_DTS (buffer) = dts;
      g_signal_emit_by_name (appsrc, "push-buffer", buffer, &ret);
    }
    gst_sample_unref (sample);
  }
}

static void
ctx_free (MyContext * ctx)
{
  g_print ("Off stream video 640\n");
  gst_element_unlink_many(tee, queue_640, videoconvert_640_c, cairo_overlay_640, videoconvert_640, videoscale_640, filter_640, x264enc_640, appsink_640, NULL);

  gst_bin_remove(GST_BIN (pipeline), queue_640);
  gst_bin_remove(GST_BIN (pipeline), videoconvert_640_c);
  gst_bin_remove(GST_BIN (pipeline), cairo_overlay_640);
  gst_bin_remove(GST_BIN (pipeline), videoconvert_640);
  gst_bin_remove(GST_BIN (pipeline), videoscale_640);
  gst_bin_remove(GST_BIN (pipeline), filter_640);
  gst_bin_remove(GST_BIN (pipeline), x264enc_640);
  gst_bin_remove(GST_BIN (pipeline), appsink_640);

  gst_element_set_state(GST_ELEMENT(queue_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(videoconvert_640_c), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(cairo_overlay_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(videoconvert_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(videoscale_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(filter_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(x264enc_640), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(appsink_640), GST_STATE_NULL);

  gst_object_unref(GST_OBJECT(queue_640));
  gst_object_unref(GST_OBJECT(videoconvert_640_c));
  gst_object_unref(GST_OBJECT(cairo_overlay_640));
  gst_object_unref(GST_OBJECT(videoconvert_640));
  gst_object_unref(GST_OBJECT(videoscale_640));
  gst_object_unref(GST_OBJECT(filter_640));
  gst_object_unref(GST_OBJECT(x264enc_640));
  gst_object_unref(GST_OBJECT(appsink_640));

  gst_object_unref (ctx->vid_appsrc );
  gst_object_unref (ctx->vid_appsink );
  g_free (ctx);
}

static void
ctx_free1 (MyContext * ctx)
{
  g_print ("Off stream video 320\n");

  gst_element_unlink_many(tee, queue_320, videoconvert_320_c, cairo_overlay_320, videoconvert_320, videoscale_320, filter_320, x264enc_320, appsink_320, NULL);

  gst_bin_remove(GST_BIN (pipeline), queue_320);
  gst_bin_remove(GST_BIN (pipeline), videoconvert_320_c);
  gst_bin_remove(GST_BIN (pipeline), cairo_overlay_320);
  gst_bin_remove(GST_BIN (pipeline), videoconvert_320);
  gst_bin_remove(GST_BIN (pipeline), videoscale_320);
  gst_bin_remove(GST_BIN (pipeline), filter_320);
  gst_bin_remove(GST_BIN (pipeline), x264enc_320);
  gst_bin_remove(GST_BIN (pipeline), appsink_320);

  gst_element_set_state(GST_ELEMENT(queue_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(videoconvert_320_c), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(cairo_overlay_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(videoconvert_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(videoscale_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(filter_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(x264enc_320), GST_STATE_NULL);
  gst_element_set_state(GST_ELEMENT(appsink_320), GST_STATE_NULL);

  gst_object_unref(GST_OBJECT(queue_320));
  gst_object_unref(GST_OBJECT(videoconvert_320_c));
  gst_object_unref(GST_OBJECT(cairo_overlay_320));
  gst_object_unref(GST_OBJECT(videoconvert_320));
  gst_object_unref(GST_OBJECT(videoscale_320));
  gst_object_unref(GST_OBJECT(filter_320));
  gst_object_unref(GST_OBJECT(x264enc_320));
  gst_object_unref(GST_OBJECT(appsink_320));
  
  
  gst_object_unref (ctx->vid_appsrc );
  gst_object_unref (ctx->vid_appsink );
  g_free (ctx);
}

static void
media_configure (GstRTSPMediaFactory * factory, GstRTSPMedia * media, gpointer user_data)
{
    GstElement *element, *appsrc;
    GstCaps *caps, *caps1;
    MyContext *ctx;

    CairoOverlayState *overlay_state;
    overlay_state = g_new0 (CairoOverlayState, 1);
    overlay_state->resolution = (char*)"640x480";

    g_print ("On stream video 640\n");
    ctx = g_new0 (MyContext, 1);

    /* make sure the data is freed when the media is gone */
    g_object_set_data_full (G_OBJECT (media), "rtsp-extra-data", ctx, (GDestroyNotify) ctx_free);

    queue_640           = gst_element_factory_make("queue", NULL);
    videoconvert_640_c  = gst_element_factory_make("videoconvert", NULL);
    cairo_overlay_640   = gst_element_factory_make("cairooverlay", NULL);
    videoconvert_640    = gst_element_factory_make("videoconvert", NULL);
    videoscale_640      = gst_element_factory_make("videoscale", NULL);
    filter_640          = gst_element_factory_make("capsfilter",NULL);
    x264enc_640         = gst_element_factory_make("x264enc", NULL);
    ctx->vid_appsink    = appsink_640 = gst_element_factory_make("appsink", "vid");
    
    caps_640 = gst_caps_from_string("video/x-raw,width=640,height=480,framerate=30/1");

    g_object_set (G_OBJECT(filter_640), "caps", caps_640, NULL);
    g_object_set (x264enc_640, "tune", 4, NULL);
    g_object_set (x264enc_640, "speed-preset", 2, NULL);
    g_object_set (appsink_640, "drop", 1, NULL);

    gst_element_set_state (pipeline, GST_STATE_READY);
    gst_element_set_state (pipeline, GST_STATE_PAUSED);

    g_signal_connect (cairo_overlay_640, "draw", G_CALLBACK (draw_overlay), overlay_state);
    g_signal_connect (cairo_overlay_640, "caps-changed", G_CALLBACK (prepare_overlay), overlay_state); 

    gst_bin_add_many (GST_BIN (pipeline),
                      queue_640, videoconvert_640_c, cairo_overlay_640, videoconvert_640, videoscale_640, filter_640, x264enc_640, appsink_640, NULL);
    gst_element_link_many(tee, queue_640, videoconvert_640_c, cairo_overlay_640, videoconvert_640, videoscale_640, filter_640, x264enc_640, appsink_640, NULL);

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    element = gst_rtsp_media_get_element (media);

    caps = gst_caps_new_simple ("video/x-h264",
                                "stream-format", G_TYPE_STRING, "byte-stream",
                                "width", G_TYPE_INT, 640, "height", G_TYPE_INT, 480,
                                "framerate", GST_TYPE_FRACTION, 30, 1, NULL);

    ctx->vid_appsrc  = appsrc  = gst_bin_get_by_name_recurse_up (GST_BIN (element), "videosrc");
    gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
    g_object_set (G_OBJECT (appsink_640), "caps", caps, NULL);
    g_object_set (G_OBJECT (appsrc), "caps", caps, NULL);
    g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);
    gst_caps_unref (caps);
    gst_object_unref (element);
}

static void
media_configure1 (GstRTSPMediaFactory * factory, GstRTSPMedia * media, gpointer user_data)
{
    GstElement *element, *appsrc;
    GstCaps *caps;
    MyContext *ctx;

    CairoOverlayState *overlay_state;
    overlay_state = g_new0 (CairoOverlayState, 1);
    overlay_state->resolution = (char*)"320x240";

    g_print ("On stream video 320\n");
    ctx = g_new0 (MyContext, 1);

    /* make sure the data is freed when the media is gone */
    g_object_set_data_full (G_OBJECT (media), "rtsp-extra-data", ctx, (GDestroyNotify) ctx_free1);

    queue_320           = gst_element_factory_make("queue", NULL);
    videoconvert_320_c  = gst_element_factory_make("videoconvert", NULL);
    cairo_overlay_320   = gst_element_factory_make("cairooverlay", NULL);
    videoconvert_320    = gst_element_factory_make("videoconvert", NULL);
    videoscale_320      = gst_element_factory_make("videoscale", NULL);
    filter_320          = gst_element_factory_make("capsfilter",NULL);
    x264enc_320         = gst_element_factory_make("x264enc", NULL);
    ctx->vid_appsink    = appsink_320 = gst_element_factory_make("appsink", "vid1");
    
    caps_320 = gst_caps_from_string("video/x-raw,width=320,height=240,framerate=30/1");
    g_object_set(G_OBJECT(filter_320), "caps", caps_320, NULL);
    g_object_set (x264enc_320, "tune", 4, NULL);
    g_object_set (x264enc_320, "speed-preset", 2, NULL);
    g_object_set (appsink_320, "drop", 1, NULL);

    gst_element_set_state (pipeline, GST_STATE_READY);
    gst_element_set_state (pipeline, GST_STATE_PAUSED);

    g_signal_connect (cairo_overlay_320, "draw", G_CALLBACK (draw_overlay), overlay_state);
    g_signal_connect (cairo_overlay_320, "caps-changed", G_CALLBACK (prepare_overlay), overlay_state); 

    gst_bin_add_many (GST_BIN (pipeline),
                      queue_320, videoconvert_320_c, cairo_overlay_320, videoconvert_320, videoscale_320, filter_320, x264enc_320, appsink_320, NULL);
    gst_element_link_many(tee, queue_320, videoconvert_320_c, cairo_overlay_320, videoconvert_320, videoscale_320, filter_320, x264enc_320, appsink_320, NULL);

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    element = gst_rtsp_media_get_element (media);

    caps = gst_caps_new_simple ("video/x-h264",
                                "stream-format", G_TYPE_STRING, "byte-stream",
                                "width", G_TYPE_INT, 320, "height", G_TYPE_INT, 240,
                                "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
    
    ctx->vid_appsrc  = appsrc  = gst_bin_get_by_name_recurse_up (GST_BIN (element), "videosrc1");
    // ctx->vid_appsink = appsink = gst_bin_get_by_name (GST_BIN (pipeline), "vid1");
    gst_util_set_object_arg (G_OBJECT (appsrc), "format", "time");
    g_object_set (G_OBJECT (appsrc), "caps", caps, NULL);
    g_object_set (G_OBJECT (appsink_320), "caps", caps, NULL);
    
    g_signal_connect (appsrc, "need-data", (GCallback) need_data, ctx);
    gst_caps_unref (caps);
    gst_object_unref (element);
}

int 
check_stt_video(){
    int stt;
    std::string data;
    FILE * stream;
    const int max_buffer = 256;
    char buffer[max_buffer];
    std::string cmd = "ls ../Videos";
    char *check = (char*) malloc(100*sizeof(char));
    stream = popen(cmd.c_str(), "r");
    if (stream) {
    while (!feof(stream))
        if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
    pclose(stream);
    }
    // vector<string> buf = split(data, ' ');
    // printf("result %s \n", data.c_str());
    for (stt=0;stt<1000;stt++){
        sprintf(check, "record_%d.mp4",stt);
        if (data.find(check) == std::string::npos){
            return stt;
        }   
    }
    return 0;
}

int 
check_stt_image(){
    int stt;
    std::string data;
    FILE * stream;
    const int max_buffer = 256;
    char buffer[max_buffer];
    std::string cmd = "ls ../Picture";
    char *check = (char*) malloc(100*sizeof(char));
    stream = popen(cmd.c_str(), "r");
    if (stream) {
    while (!feof(stream))
        if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
    pclose(stream);
    }
    // vector<string> buf = split(data, ' ');
    // printf("result %s \n", data.c_str());
    for (stt=0;stt<1000;stt++){
        sprintf(check, "capture_%d.jpeg",stt);
        if (data.find(check) == std::string::npos){
            return stt;
        }   
    }
    return 0;
}

void 
delete_file_record(){
    char *cmdstr = (char*) malloc(100*sizeof(char));
    sprintf(cmdstr, "rm -rf ../Videos/*");
    int32_t ret;
    ret = system(cmdstr);
    if (ret==0) g_print("Deleted all file record...\n\n");
    stt_record = check_stt_video();

}

void
record_start()
{
    queue_record        = gst_element_factory_make("queue", "queue_record");
    videoconvert_record = gst_element_factory_make("videoconvert","convert_record");
    x264enc_record      = gst_element_factory_make("x264enc","x264");
    mp4mux_record       = gst_element_factory_make("mp4mux","mp4mux");
    filesink_record     = gst_element_factory_make("filesink", "filesink");

    sprintf(file_record, "%s/record_%d.mp4", folder_record, stt_record++);

    g_object_set (filesink_record, "location", file_record, NULL);
    g_object_set (x264enc_record, "tune", 4, NULL);
    gst_element_set_state (pipeline, GST_STATE_READY);
    gst_element_set_state (pipeline, GST_STATE_PAUSED);
    gst_bin_add_many (GST_BIN (pipeline),
                        queue_record, videoconvert_record, x264enc_record, mp4mux_record, filesink_record, NULL);
    gst_element_link_many(tee, queue_record, videoconvert_record, x264enc_record, mp4mux_record, filesink_record, NULL);

    g_print ("~~~%s Record~~~\n",recording ? "Start" : "Stop");
    g_print ("-->Recording to record_%d.mp4...\n\n",stt_record-1);

    gst_element_set_state (pipeline, GST_STATE_PLAYING);
}

void record_stop()
{
    gst_element_send_event(mp4mux_record, gst_event_new_eos());

    gst_element_set_state (queue_record, GST_STATE_NULL);
    gst_element_set_state (videoconvert_record, GST_STATE_NULL);
    gst_element_set_state (x264enc_record, GST_STATE_NULL);
    gst_element_set_state (mp4mux_record, GST_STATE_NULL);
    gst_element_set_state (filesink_record, GST_STATE_NULL);

    gst_element_unlink_many(tee, queue_record, videoconvert_record, x264enc_record, mp4mux_record, filesink_record, NULL);
    gst_bin_remove_many (GST_BIN (pipeline),
                        queue_record, videoconvert_record, x264enc_record, mp4mux_record, filesink_record, NULL);

    g_print ("~~~%s Record~~~\n",recording ? "Start" : "Stop");
    g_print ("-->Recorded to record_%d.mp4\n\n\n",stt_record-1);
}

void captureImage()
{
    queue_capture        = gst_element_factory_make("queue", NULL);
    videoconvert_capture = gst_element_factory_make("videoconvert", NULL);
    jpegenc_capture      = gst_element_factory_make("jpegenc",NULL);
    filesink_capture     = gst_element_factory_make("filesink", NULL);

    sprintf(file_capture, "%s/capture_%d.jpeg", folder_capture, stt_capture++);

    g_object_set (filesink_capture, "location", file_capture, NULL);
    // g_object_set (jpegenc_capture, "tune", 4, NULL);

    gst_element_set_state (pipeline, GST_STATE_READY);
    gst_element_set_state (pipeline, GST_STATE_PAUSED);

    gst_bin_add_many (GST_BIN (pipeline),
                        queue_capture, videoconvert_capture, jpegenc_capture, filesink_capture, NULL);
    gst_element_link_many(tee, queue_capture, videoconvert_capture, jpegenc_capture, filesink_capture, NULL);

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    g_print ("-->Capture to capture_%d.jpeg\n\n\n",stt_capture-1);
    
    usleep(1000000);

    gst_element_set_state (queue_capture, GST_STATE_NULL);
    gst_element_set_state (videoconvert_capture, GST_STATE_NULL);
    gst_element_set_state (jpegenc_capture, GST_STATE_NULL);
    gst_element_set_state (filesink_capture, GST_STATE_NULL);

    gst_element_unlink_many(tee, queue_capture, videoconvert_capture, jpegenc_capture, filesink_capture, NULL);
    gst_bin_remove_many (GST_BIN (pipeline),
                        queue_capture, videoconvert_capture, jpegenc_capture, filesink_capture, NULL);
    g_print ("--DONE ....\n");
}

static gboolean
handle_keyboard (GIOChannel * source, GIOCondition cond, gpointer data)
{
    gchar *str = NULL;
    if (g_io_channel_read_line (source, &str, NULL, NULL,
                                NULL) != G_IO_STATUS_NORMAL) {
        return TRUE;
    }

    switch (g_ascii_tolower (str[0])) {
        case 'r': 
            // RECORD VIDEO
            recording = !recording;
            if (recording){
                record_start();
            }
            else {
                record_stop();
            }
            break;
        
        case 'c':
            // CAPTURE IMAGE
            captureImage();
            break;

        case 'd':
            delete_file_record();
            break;

        default:
            break;
    }
    g_free (str);
    return TRUE;
}

int main (int argc, char *argv[])
{
    GMainLoop             *loop;
    GstRTSPServer         *server;
    GstRTSPMountPoints    *mounts;
    GstRTSPMediaFactory   *factory, *factory1;
    GIOChannel            *io_stdin;

    stt_record  = check_stt_video();
    stt_capture = check_stt_image();
    ip = check_IP();
    gst_init (&argc, &argv);
    loop    = g_main_loop_new (NULL, FALSE);
    server  = gst_rtsp_server_new ();
    g_object_set(server, "service", "5004", NULL);
    
    mounts  = gst_rtsp_server_get_mount_points (server);
    factory = gst_rtsp_media_factory_new ();
    factory1= gst_rtsp_media_factory_new ();

    pipeline = gst_parse_launch ("v4l2src device=/dev/video0 name=source ! video/x-raw,width=640,height=480,framerate=30/1 \
                                ! tee name=t t. ! queue ! videoconvert ! fakesink " , NULL);

    tee = gst_bin_get_by_name (GST_BIN (pipeline), "t");

    gst_element_set_state (pipeline, GST_STATE_PLAYING);

    gst_rtsp_media_factory_set_launch (factory,
        "( appsrc name=videosrc is-live=true ! h264parse ! rtph264pay name=pay0 pt=96 )");
    gst_rtsp_media_factory_set_shared(factory, TRUE);
    g_signal_connect (factory, "media-configure", (GCallback) media_configure, NULL);
    gst_rtsp_mount_points_add_factory (mounts, "/test", factory);

    gst_rtsp_media_factory_set_launch (factory1,
        "( appsrc name=videosrc1 is-live=true ! h264parse ! rtph264pay name=pay0 pt=96 )");
    gst_rtsp_media_factory_set_shared(factory1, TRUE);
    g_signal_connect (factory1, "media-configure", (GCallback) media_configure1, NULL);
    gst_rtsp_mount_points_add_factory (mounts, "/test1", factory1);

    g_object_unref (mounts);

    gst_rtsp_server_attach (server, NULL);

    io_stdin = g_io_channel_unix_new (fileno (stdin));
    g_io_add_watch (io_stdin, G_IO_IN, (GIOFunc) handle_keyboard, loop);

    g_print ( "Stream ready at rtsp://127.0.0.1:5004/test  -- Stream video 640x480\n"
              "Stream ready at rtsp://127.0.0.1:5004/test1 -- Stream video 320x240\n\n");
    g_main_loop_run (loop);
    return 0;
}