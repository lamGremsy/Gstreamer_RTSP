# Gstreamer_RTSP

## Create folder Record and Capture
```
 $ mkdir Videos
 $ mkdir Picture
```

## Build and run example
### Build example
```
 $ mkdir build
 $ cd build
 $ cmake ..
 $ make
```
### Run example
```
 $ ./rtsp_twostream_record_capture
 ...
```

## Get stream from RTSP
### Thread 1
```
 $ gst-launch-1.0 rtspsrc latency=0 location=rtsp://127.0.0.1:5004/test ! rtph264depay ! decodebin ! videoconvert ! autovideosink sync=0
```
### Thread 2
```
 $ gst-launch-1.0 rtspsrc latency=0 location=rtsp://127.0.0.1:5004/test1 ! rtph264depay ! decodebin ! videoconvert ! autovideosink sync=0
```

## Keyboard event
```
 1. "q" - exit stream.
 2. "m" - change mode stream
 3. "r" - record video
 4. "c" - capture
 5. "d" - delete all record file and capture file
```

## Example Picture in Picture HDMI on Terminal
```
 $ gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw, framerate=30/1, width=640, height=480 \
 ! videomixer name=mix sink_0::zorder=1 sink_1::zorder=0 \
 ! autovideosink sync=0 \
 v4l2src device=/dev/video2 ! video/x-raw, framerate=30/1, width=640, height=480 \
 ! videoconvert ! mix.
```

## Review Source Code
```
 none
```