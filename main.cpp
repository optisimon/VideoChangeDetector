/*
 * main.cpp
 *
 *  Created on: Aug 4, 2016
 *
 *  Copyright (c) 2016 Simon Gustafsson (www.optisimon.com)
 *  Do whatever you like with this code, but please refer to me as the original author.
 */

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>

#include <assert.h>
#include <getopt.h>
#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atomic>


int verboseFlag = 0;
gchar* inputVideoFilename = NULL;

static std::atomic<double> threshold(10.0);

const gchar *appsinkVideoCaps = "video/x-raw,format=GRAY8";

typedef struct
{
	GMainLoop *mainLoop;
	GstElement *source;
} ProgramData;


//
// Called when the appsink notifies us that there is a new buffer ready for processing
//
static GstFlowReturn on_new_sample_from_sink(GstElement * element, ProgramData * data)
{
	GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK (element));
	GstBuffer *buffer = gst_sample_get_buffer(sample);
	GstCaps *caps = gst_sample_get_caps(sample);

	GstVideoInfo videoInfo;
	gst_video_info_from_caps(&videoInfo, caps);

	GstVideoFrame videoFrame;
	gboolean isMapped = gst_video_frame_map(&videoFrame, &videoInfo, buffer, GST_MAP_READ);

	if (isMapped)
	{
		gpointer pixels = GST_VIDEO_FRAME_PLANE_DATA(&videoFrame, 0);
		guint stride = GST_VIDEO_FRAME_PLANE_STRIDE(&videoFrame, 0);
		guint w = GST_VIDEO_INFO_WIDTH(&videoInfo);
		guint h = GST_VIDEO_INFO_HEIGHT(&videoInfo);

		uint8_t* p = (uint8_t*)pixels;
		long sum = 0;
		for (unsigned y = 0; y < h; y++)
		{
			for (unsigned x = 0; x < w; x++)
			{
				sum += p[x + y*stride];
			}
		}

		assert(GST_BUFFER_PTS_IS_VALID(buffer) && "No PTS timestamp known!");
		GstClockTime pts = GST_BUFFER_PTS(buffer);

		static long old_sum = 0;
		double avg = sum * 1.0 / w / h;
		double avg_delta = (sum - old_sum) * 1.0 / w / h;
		double pts_sec = pts / 1000000000.0;
		old_sum = sum;

		if (verboseFlag)
		{
			g_print("AVG: %8.3f, AVG_DELTA:%8.3f, pts(sec):%8.3f\n",
					avg,
					avg_delta,
					pts_sec
			);
		}

		if (fabs(avg_delta) > threshold)
		{
			int hours = pts_sec / (60 * 60);
			int minutes = (pts_sec - hours * (60*60)) / 60;
			int seconds = pts_sec - hours * (60*60) - minutes * 60;

			g_print("LIGHTNING: pts(sec)=%8.3f (%d:%02d:%02d)\n",
					pts_sec,
					hours,
					minutes,
					seconds
			);
		}
		gst_video_frame_unmap(&videoFrame);
	}

	gst_sample_unref(sample);

	return GST_FLOW_OK;
}


// EndOfStream or gstreamer errors in our pipeline should quit our main thread
static gboolean on_source_message(GstBus * bus, GstMessage * message, ProgramData * data)
{
	switch (GST_MESSAGE_TYPE (message))
	{
	case GST_MESSAGE_EOS:
		g_print("The source got dry\n");
		g_main_loop_quit(data->mainLoop);
		break;

	case GST_MESSAGE_ERROR:
		g_print("Received error\n");
		g_main_loop_quit(data->mainLoop);
		break;

	default:
		break;
	}
	return TRUE;
}


int main (int argc, char *argv[])
{
	int showHelpFlag = 0;
	inputVideoFilename = g_strdup("/home/simon/git/VideoChangeDetector/GP010030.MP4");


	//
	// Handle command line arguments
	//
	while(true)
	{
		int c;
		static struct option long_options[] =
		{
				// These options set a flag.
				{"verbose", no_argument,   &verboseFlag, 1},
				{"brief",   no_argument,   &verboseFlag, 0},
				{"help",    no_argument,   &showHelpFlag, 1},

				// These options don’t set a flag. We distinguish them by their indices.
				{"file",    required_argument, 0, 'f'},
				{"threshold", optional_argument, 0, 't'},
				{0, 0, 0, 0}
		};
		// getopt_long stores the option index here.
		int option_index = 0;
		c = getopt_long(argc, argv, "vbhf:t:", long_options, &option_index);

		// Detect the end of the options.
		if (c == -1)
			break;

		switch (c)
		{
		case 0:
			// If this option set a flag, do nothing else now.
			if (long_options[option_index].flag != 0)
			{
				break;
			}
			printf("*option %s", long_options[option_index].name);
			if (optarg)
			{
				printf(" with arg %s", optarg);
			}
			printf("\n");
			break;

		case 'v':
			verboseFlag = 1;
			break;

		case 'b':
			verboseFlag = 0;
			break;

		case 'h':
			showHelpFlag = 1;
			break;

		case 'f':
			printf("Using \"%s\" as input video file\n", optarg);
			inputVideoFilename = g_strdup(optarg);
			break;

		case 't':
			printf("Using threshold \"%s\"\n", optarg);
			threshold = atof(optarg); // TODO: error checking
			break;

		case '?':
			// getopt_long already printed an error message.
			break;

		default:
			abort();
		}
	}

	if (showHelpFlag)
	{
		printf(
				"Usage: %s [options] [-f input_video_filename]\n"
				"  -v, --verbose                Show additional debugging output\n"
				"  -b, --brief                  Do not show additional debugging output (default)\n"
				"  -h, --help                   Show this help text\n"
				"  -f FILE, --file FILE         Specify input video file to look for flashes in\n"
				"  -t value, --threshold value  Average global intensity increase above given value\n"
				"                               is counted as a lightning event\n"
				"\n", basename(argv[0])
		);
		return 1;
	}


	ProgramData *programData = g_new0(ProgramData, 1);
	assert(programData);
	programData->mainLoop = g_main_loop_new(NULL, FALSE);


	//
	// Create gstreamer pipeline string.
	// * we read from a file using uridecodebin, so we could read any video file gstreamer supports.
	// * videoconvert converts the video format to "obey" the caps of the appsink (if needed)
	// * appsink is the receiver
	//
	// TODO: Check filename for gstreamer pipeline injection?
	//
	char scratchpad[PATH_MAX + 1];
	char *actualRealFilename = realpath(inputVideoFilename, scratchpad);
	assert(actualRealFilename && "Could not convert input filename to a valid absolute path");
	gchar * pipelineString = g_strdup_printf(
			"uridecodebin uri=\"file://%s\" ! videoconvert ! appsink caps=\"%s\" name=testsink",
			actualRealFilename,
			appsinkVideoCaps
	);
	if (verboseFlag)
	{
		g_print("Pipeline=\"%s\"\n", pipelineString);
	}
	g_free(inputVideoFilename);


	//
	// Set up the gstreamer pipeline
	//
	gst_init(&argc, &argv);
	programData->source = gst_parse_launch(pipelineString, NULL);
	g_free(pipelineString);

	if (programData->source == NULL)
	{
		g_print("Bad source\n");
		return 2;
	}


	//
	// We want to quit on EndOfStream or pipeline errors, so we have to watch our bus
	//
	GstBus *bus = gst_element_get_bus(programData->source);
	gst_bus_add_watch(bus, (GstBusFunc) on_source_message, programData);
	gst_object_unref(bus);


	//
	// We use appsink in push mode. It sends us a signal when data is available
	// and we pull out the data in the signal callback. We want the appsink to
	// push as fast as it can, hence the sync=false
	//
	GstElement *testSink = gst_bin_get_by_name(GST_BIN(programData->source), "testsink");
	assert(testSink);
	g_object_set(G_OBJECT(testSink), "emit-signals", TRUE, "sync", FALSE, NULL);
	g_signal_connect(testSink, "new-sample", G_CALLBACK(on_new_sample_from_sink), programData);
	gst_object_unref(testSink);


	//
	// Run the pipeline.
	// Main loop will quit when the pipeline goes EndOfStream or when an error occurs in the pipeline.
	//
	if (verboseFlag)
	{
		g_print("Starting to crunch frames\n");
	}
	gst_element_set_state(programData->source, GST_STATE_PLAYING);
	g_main_loop_run(programData->mainLoop);
	if (verboseFlag)
	{
		g_print("Stopped crunching frames\n");
	}

	// Some cleanup
	gst_element_set_state(programData->source, GST_STATE_NULL);
	gst_object_unref(programData->source);
	g_main_loop_unref(programData->mainLoop);
	g_free(programData);

	return 0;
}
