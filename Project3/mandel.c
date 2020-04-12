#include "bitmap.h"
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>



int iteration_to_color( int i, int max, double x, double y, float pixels);
int iterations_at_point( double x, double y, int max, double xmax, double ymax );
void compute_image( struct bitmap *bm, double xmin, double xmax, double ymin, double ymax, int max, int thrd);

void show_help()
{
	printf("Use: mandel [options]\n");
	printf("Where options are:\n");
	printf("-m <max>    The maximum number of iterations per point. (default=1000)\n");
	printf("-x <coord>  X coordinate of image center point. (default=0)\n");
	printf("-y <coord>  Y coordinate of image center point. (default=0)\n");
	printf("-s <scale>  Scale of the image in Mandlebrot coordinates. (default=4)\n");
	printf("-W <pixels> Width of the image in pixels. (default=500)\n");
	printf("-H <pixels> Height of the image in pixels. (default=500)\n");
	printf("-o <file>   Set output file. (default=mandel.bmp)\n");
	printf("-h          Show this help text.\n");
	printf("\nSome examples are:\n");
	printf("mandel -x -0.5 -y -0.5 -s 0.2\n");
	printf("mandel -x -.38 -y -.665 -s .05 -m 100\n");
	printf("mandel -x 0.286932 -y 0.014287 -s .0005 -m 1000\n\n");
}

int main( int argc, char *argv[] )
{
	char c;

	// These are the default configuration values used
	// if no command line arguments are given.

	const char *outfile = "mandel.bmp";
	double xcenter = 0;
	double ycenter = 0;
	double scale = 4;
	int    image_width = 500;
	int    image_height = 500;
	int    max = 1000;
	int    thrd = 1;

	// For each command line argument given,
	// override the appropriate configuration value.

	while((c = getopt(argc,argv,"x:y:s:W:H:m:o:n:h"))!=-1) {
		switch(c) {
			case 'x':
				xcenter = atof(optarg);
				break;
			case 'y':
				ycenter = atof(optarg);
				break;
			case 's':
				scale = atof(optarg);
				break;
			case 'W':
				image_width = atoi(optarg);
				break;
			case 'H':
				image_height = atoi(optarg);
				break;
			case 'm':
				max = atoi(optarg);
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'n':
				thrd = atoi(optarg);
				break;
			case 'h':
				show_help();
				exit(1);
				break;
		}
	}

	//printf("Running with %i threads\n", thrd);
	// Display the configuration of the image.
	printf("mandel: x=%lf y=%lf scale=%lf max=%d outfile=%s\n",xcenter,ycenter,scale,max,outfile);

	// Create a bitmap of the appropriate size.
	struct bitmap *bm = bitmap_create(image_width,image_height);

	// Fill it with a dark blue, for debugging
	bitmap_reset(bm,MAKE_RGBA(0,0,255,0));

	// Compute the Mandelbrot image
	compute_image(bm,xcenter-scale,xcenter+scale,ycenter-scale,ycenter+scale,max,thrd);

	// Save the image in the stated file.
	if(!bitmap_save(bm,outfile)) {
		fprintf(stderr,"mandel: couldn't write to %s: %s\n",outfile,strerror(errno));
		return 1;
	}

	return 0;
}



typedef struct{
	struct bitmap *bm;
	double xmin;
	double xmax;
	double ymin;
	double ymax;
	int max;
	int TID;
	int thrd;
} myarg_t;

void* mythread(void* allArgs){

	myarg_t args = *((myarg_t *) allArgs);

	int i,j;

	int width = bitmap_width(args.bm);
	int height = bitmap_height(args.bm);

	//The calculation that this uses is the following:
	// Thread Start = (current thread ID)/(total number of threads) x height
	// Thread End = (current thread ID + 1)/(total number of threads) x height
	// The order of operations is changed in the loop to avoid integer division truncation
	// Essentially its breaking the height up into sections based on the Thread ID
	// To check start and end point uncomment the following printf statements

	//printf("TID %i starting point:%i\n",args.TID ,(args.TID*height) / args.thrd);
	//printf("TID %i ending point: %i\n", args.TID,((args.TID+1)*height) / args.thrd);

	for(j= (args.TID*height) / args.thrd ;j< ((args.TID+1)*height) / args.thrd;j++) {

		for(i=0;i<width;i++) {

			// Determine the point in x,y space for that pixel.
			double x = args.xmin + i*(args.xmax-args.xmin)/width;
			double y = args.ymin + j*(args.ymax-args.ymin)/height;

			// Compute the iterations at that point.
			int iters = iterations_at_point(x,y,args.max, args.xmax, args.ymax);

			// Set the pixel in the bitmap.
			bitmap_set(args.bm,i,j,iters);
		}
	}
	
	return NULL;
}


/*
Compute an entire Mandelbrot image, writing each point to the given bitmap.
Scale the image to the range (xmin-xmax,ymin-ymax), limiting iterations to "max"
*/

void compute_image( struct bitmap *bm, double xmin, double xmax, double ymin, double ymax, int max, int thrd)
{

	pthread_t my_threads[thrd];
	//int TID[thrd];
	myarg_t args[thrd];


	for(int i=0; i < thrd; i++){
		
		args[i].bm = bm;
		args[i].xmin = xmin;
		args[i].xmax = xmax;
		args[i].ymin = ymin;
		args[i].ymax = ymax;
		args[i].max = max;
		args[i].thrd = thrd;
		args[i].TID = i;

		pthread_create(&my_threads[i], NULL, mythread, &args[i]);
	}

	for(int i=0; i < thrd; i++){
		pthread_join(my_threads[i], NULL);
	}

}

/*
Return the number of iterations at point x, y
in the Mandelbrot space, up to a maximum of max.
*/

int iterations_at_point( double x, double y, int max, double xmax, double ymax )
{
	double x0 = x;
	double y0 = y;

	int iter = 0;

	while( (x*x + y*y <= 4) && iter < max ) {

		double xt = x*x - y*y + x0;
		double yt = 2*x*y + y0;

		x = xt;
		y = yt;

		iter++;
	}

	return iteration_to_color(iter,max, x, y, xmax * ymax);
}

/*
Convert a iteration number to an RGBA color.
Here, we just scale to gray with a maximum of imax.
Modify this function to make more interesting colors.
*/

int iteration_to_color( int i, int max, double x, double y, float total)
{
	/*
	For EC
	int red = 0, green = 0, blue = 0;

	
	int range = (x*y)/total;

	if(range < total/3){
		red = 255;
	}else if(range < 2*total/3){
		green = 255;
	}else{
		blue = 255;
	}
	*/

	int gray = 255*i/max;

	return MAKE_RGBA(gray,gray,gray,0);
}




