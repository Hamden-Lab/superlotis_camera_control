//Basic Acquisition Sample
//The sample will open the first camera attached
//and acquire 5 frames.  Part 2 of the sample will collect
//1 frame of data each time the function is called, looping
//through 5 times.

// #define NUM_FRAMES 2

#include "stdio.h" 
#include "picam.h"
#include <iostream>

PicamHandle camera;
pi64s total_acquired = 0;
const pi64s target_frames = 2; //replaced NUM_FRAMES with target_frames
piint readoutstride = 0;
PicamCameraID id;
const pichar* string;
piint status;
PicamAvailableData data;
PicamAcquisitionErrorsMask errors;
piflt exposure_time = 1000;

int PrintEnumString(PicamEnumeratedType type, piint value)
{
    const pichar* string;
    PicamError error = Picam_GetEnumerationString(type, value, &string);
    if (error == PicamError_None)
    {
        printf("%s", string);
        Picam_DestroyString(string);
        return 0;  // Success
    }
    else
    {
        return -1; // Failure
    }
}

int PrintError(PicamError error)
{
    if (error == PicamError_None){
        printf("Succeeded\n");
        return 0;
    }
    else
    {
        printf("Failed (");
        PrintEnumString(PicamEnumeratedType_Error, error);
        printf(")\n");
        return -1;
    }
}

void PrintData( pibyte* buf, piint numframes, piint framelength )
{
    pi16u  *midpt = NULL;
    pibyte *frameptr = NULL;

    for( piint loop = 0; loop < numframes; loop++ )
    {
        frameptr = buf + ( framelength * loop );
        midpt = (pi16u*)frameptr + ( ( ( framelength/sizeof(pi16u) )/ 2 ) );
        printf( "%5d,%5d,%5d\t%d\n", (int) *(midpt-1), (int) *(midpt),  (int) *(midpt+1), (int) loop+1 );
    }
}

// Define status enum
enum CameraStatus {
    Exposing = 0,
    Idle = 1,
    Readout = 2
} CameraStatus;

// Define the function
int get_status(pi64s total_acquired, pi64s target_frames) {
    if (total_acquired == 0){
        return Idle;
    }
    else if (total_acquired < target_frames){
        return Exposing;
    }
    else{
        return Readout;
    }
}

const char* status_to_string(piint status) {
    switch (status) {
        case Idle: return "Idle";
        case Exposing: return "Exposing";
        case Readout: return "Readout";
        default: return "Unknown";
    }
}

int open_camera()
{
    Picam_InitializeLibrary();
    printf("lib initialzed\n");

    printf("starting...\n");
    if( Picam_OpenFirstCamera( &camera ) == PicamError_None )
        Picam_GetCameraID( camera, &id );
    else
    {
        Picam_ConnectDemoCamera(
            PicamModel_Sophia2048BUV135, 
            "XO30000923",
            &id );
        PicamError err = Picam_OpenCamera( &id, &camera );
        if (err != PicamError_None) {
            printf("Failed to open demo camera\n");
            return -1;
        }
    }

    Picam_GetEnumerationString( PicamEnumeratedType_Model, id.model, &string );
    printf( "%s", string );
    printf( " (SN:%s) [%s]\n", id.serial_number, id.sensor_name );
    Picam_DestroyString( string );

    Picam_GetParameterIntegerValue( camera, PicamParameter_ReadoutStride, &readoutstride );
    printf( "Waiting for %d frames to be collected\n\n", (int) target_frames );

    return 0;

}

int close_camera()
{
    Picam_CloseCamera( camera );
    Picam_UninitializeLibrary();
    return 0;
}

int get_exposure_time(piflt *exposure_time)
{
    printf("Getting current exposure time...\n");
    PicamError err;
    err = Picam_GetParameterFloatingPointValue(camera, PicamParameter_ExposureTime, exposure_time);
    if (err != PicamError_None)
    {
        printf("Failed to get exposure time.\n");
        PrintError(err);
        return -1;
    }
    else
    {
        // std::cout << "Exposure time is: " << *exposure_time << " ms " << std::endl;
        // printf("Exposure time is: %.2f ms\n", *(double*)exposure_time);
        return 0;
    }
}

int set_exposure_time(piflt exposure_time)
{
    printf("Setting new exposure time...\n");
    PicamError error = Picam_SetParameterFloatingPointValue(camera, PicamParameter_ExposureTime, exposure_time);
    if (error != PicamError_None)
    {
        printf("Failed to set exposure time.\n");
        PrintError(error);
        return -1;
    }
    else
    {
        // std::cout << "Exposure time set to: " << exposure_time << " ms " << std::endl;
        return 0;
    }
}

void image(){

    while (total_acquired < target_frames)
    {
        // int timeout = (int)(exposure_time * NUM_EXPOSURES + 2000); 

        PicamError err = Picam_Acquire(camera, 1, 6000, &data, &errors);
        // status = get_status(total_acquired, target_frames);
        
        if (err == PicamError_None)
        {
            PrintData( (pibyte*)data.initial_readout, 1, readoutstride );

            total_acquired += data.readout_count;
            printf("Total frames acquired so far: %ld\n", total_acquired);

            status = get_status(total_acquired, target_frames);
            printf("Status: %s\n", status_to_string(status));
        }
        else
        {
            printf("Acquisition error after %ld frames\n", total_acquired);
            PrintError(err);
            break;
        }
    }

}

int main()
{
    open_camera();
    piflt new_time = 50.0;
    piflt retrieved_time;
    set_exposure_time(new_time);
    get_exposure_time(&retrieved_time);
    std::cout << "exptime in main" << retrieved_time << std::endl;
    status = get_status(total_acquired, target_frames);
    printf("Status: %s\n", status_to_string(status));
    image();

    close_camera();

}


// if( Picam_Acquire( camera, NUM_FRAMES, 6000, &data, &errors ) )
//         printf( "Error: Camera only collected %d frames\n", (int) data.readout_count );
//     else
//     {    
//         printf( "Center Three Points:\tFrame # \n");
//         PrintData( (pibyte*)data.initial_readout, NUM_FRAMES, readoutstride );
//     }

//     printf( "\n\n" );
//     printf( "Collecting 1 frame, looping %d times\n\n", (int) NUM_FRAMES );
//     for( piint i = 0; i < NUM_FRAMES; i++ )
//     {
//         if( Picam_Acquire( camera, 1, 6000, &data, &errors ) )
//             printf( "Error: Camera only collected %d frames\n", (int)data.readout_count );
//         else
//         {
//             PrintData( (pibyte*)data.initial_readout, 1, readoutstride );
//         }
//     }