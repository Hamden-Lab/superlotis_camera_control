/*
This script defines camera commands 
*/
#include "stdio.h"
#include "picam.h"
#include "camera.h"
#include <iostream>
#include <sstream>
#include <sys/stat.h>  // For stat and mkdir
#include <cerrno>      // For errno
#include <cstring>     // For strerror
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <string>
#include <cstdlib>
#include <cstdio>
extern "C" {
    #include "fitsio.h"
}

#define NO_TIMEOUT  -1
#define TIME_BETWEEN_READOUTS 10 //ms
#define NUM_EXPOSURES  2
#define EXP_TIME 6000 // Default exposure time in milliseconds
#define ACQUIRE_TIMEOUT 15000  // Fifteen-second timeout
#define OK 0
#define ERR -1
#define ELEMENT_SIZE 2 //for dtype = uint16, this is a camera property
#define WIDTH 2048
#define HEIGHT 2048

PicamPtcArgs params;


void PrintData( pibyte* buf, piint numframes, piint framelength )
{
    pi16u  *midpt = NULL;
    pibyte *frameptr = NULL;

    for( piint loop = 0; loop < numframes; loop++ )
    {
        frameptr = buf + ( framelength * loop );
        midpt = (pi16u*)frameptr + ( ( ( framelength/sizeof(pi16u) )/ 2 ) );
        printf( "%5d,%5d,%5d\t%d\n",(int) *(midpt-1),(int) *(midpt),(int) *(midpt+1),(int) loop+1 );
    }
}

int PrintEnumString(PicamEnumeratedType type, piint value)
{
    // const pichar* string;
    PicamError error = Picam_GetEnumerationString(type, value, &params.string);
    if (error == PicamError_None)
    {
        printf("%s", params.string);
        Picam_DestroyString(params.string);
        return OK;  // Success
    }
    else
    {
        return ERR; // Failure
    }
}

int PrintError(PicamError error)
{
    if (error == PicamError_None){
        printf("Succeeded\n");
        return OK;
    }
    else
    {
        printf("Failed (");
        PrintEnumString(PicamEnumeratedType_Error, error);
        printf(")\n");
        return ERR;
    }
}

int get_exposure_time(piflt *exposure_time)
{
    printf("Getting current exposure time...\n");
    PicamError err;
    // std::cout << "[DEBUG] getter - output from Picam_GetParameterFloatingPointValue" << err << std::endl;

    err = Picam_GetParameterFloatingPointValue(params.camera, PicamParameter_ExposureTime, &params.exposure_time);
    // std::cout << "[DEBUG] getter - output from Picam_GetParameterFloatingPointValue" << err << std::endl;

    if (err != PicamError_None)
    {
        printf("Failed to get exposure time.\n");
        PrintError(err);
        return ERR;
    }
    else
    {
        params.exposure_time = *exposure_time;

        return OK;

    }

}


int set_exposure_time(piflt exposure_time)
{

    params.exposure_time = exposure_time;

    printf("Setting new exposure time...\n");

    PicamError error = Picam_SetParameterFloatingPointValue(params.camera, PicamParameter_ExposureTime, exposure_time);
    // std::cout << "[DEBUG] setter - output from Picam_GetParameterFloatingPointValue" << error << std::endl;

    if (error != PicamError_None)
    {
        printf("Failed to set exposure time.\n");
        PrintError(error);
        return ERR;
    }
    else
    {
        return OK;
    }
}

// int get_shutter(piint *mode)
// {
//     printf("Getting shutter mode...\n");
//     PicamError error;
//     error = Picam_GetParameterIntegerValue(params.camera, PicamParameter_ShutterTimingMode, &params.mode);
//     // PrintError(error);

//     if (error == PicamError_None)
//     {
//         const char* modeDescription = "Unknown shutter status";
//         switch (*(int*)params.mode) {
//             case 1:
//                 modeDescription = "normal timing mode";
//                 break;
//             case 2:
//                 modeDescription = "shutter always closed";
//                 break;
//             case 3:
//                 modeDescription = "shutter always open";
//                 break;
//             case 4:
//                 modeDescription = "shutter open before trigger";
//                 break;
//             default:
//                 break;
//         }
//         std::cout << modeDescription << std::endl;
//         return OK;
//     }
//     else
//     {
//         std::cout << "Failed to get shutter timing mode." << std::endl;
//         return ERR;
//     }
// }


// int set_shutter(piint mode)
// {
//     PicamError error;
//     printf("Setting shutter mode...\n");
//     error = Picam_SetParameterIntegerValue(params.camera, PicamParameter_ShutterTimingMode, params.mode);
//     if (error == PicamError_None){

//         return OK;
//     }
//     else
//     {
//         std::cout << "Failed to set shutter mode." << std::endl;
//         return ERR;
//     }

// }


int get_temp(piflt *temp)
{
    PicamError error;
    std::cout << "Getting sensor temperature..."<< std::endl;
    error = Picam_ReadParameterFloatingPointValue(
        params.camera,
        PicamParameter_SensorTemperatureReading,
        temp);
    
    PrintError(error);
    if (error != PicamError_None)
    {
        return ERR;
    }
    else
    {
        params.temp = *temp;
        std::cout << "*temp " << *temp << " degrees C" << std::endl;
        std::cout << "params.temp " << params.temp << " degrees C" << std::endl;

        return OK;
    }
}

int set_temp(piflt temp)
{
    PicamError error;
    std::cout << "Checking sensor temperature status..." << std::endl;
    PicamSensorTemperatureStatus status;
    error = Picam_ReadParameterIntegerValue(
        params.camera,
        PicamParameter_SensorTemperatureStatus,
        reinterpret_cast<piint*>(&status));
    PrintError(error);
    if (error == PicamError_None)
    {
        std::cout << "Status is: ";
        PrintEnumString(PicamEnumeratedType_SensorTemperatureStatus, status);
        std::cout << std::endl;

        // If temperature is locked, set the temperature set point
        if (status == PicamSensorTemperatureStatus_Locked)
        {
            std::cout << "Setting sensor temperature..." << std::endl;
            error = Picam_SetParameterFloatingPointValue(
                params.camera,
                PicamParameter_SensorTemperatureSetPoint,
                temp);
            PrintError(error);
            return OK;  // Return success if setting the temperature was successful
        }
        else
        {
            std::cout << "Temperature is not locked. Skipping setting temperature." << std::endl;
            return ERR;  // Return -1 if temperature is not locked
        }
    }
    
    std::cerr << "Failed to read sensor temperature status." << std::endl;
    return -2;  // Indicate failure in reading the sensor temperature status
}


int get_analog_gain(piint *gain)
{
    std::cout << "Getting adc analog gain..." << std::endl;
    PicamError error;
    error = Picam_GetParameterIntegerValue(params.camera, PicamParameter_AdcAnalogGain, &params.gain);

    // Print current analog gain
    if (error == PicamError_None)
    {
        const char* gainDescription = "unknown";
        switch (*(int*)gain) {
            case 1:
                gainDescription = "low";
                break;
            case 2:
                gainDescription = "medium";
                break;
            case 3:
                gainDescription = "high";
                break;
        }
        std::cout << "Current analog gain value: " << gainDescription << std::endl;
        return OK;
    }
    else
    {
        std::cout << "Failed to get analog gain." << std::endl;
        return ERR;
    }
}

/*    PicamAdcAnalogGain_Low    = 1,
    PicamAdcAnalogGain_Medium = 2,
    PicamAdcAnalogGain_High   = 3*/

int set_analog_gain(piint gain)
{
    PicamError error;
    get_analog_gain(&gain);
    printf("Setting adc analog gain...\n");

    error = Picam_SetParameterIntegerValue(params.camera, PicamParameter_AdcAnalogGain, params.gain);

    if (error == PicamError_None)
    {
        return OK;
    }
    else
    {
    return ERR;
    }
}


int open_camera()
{
    Picam_InitializeLibrary();
    std::cout << "Open camera" << std::endl;

    if (Picam_OpenFirstCamera(&params.camera) == PicamError_None)
        Picam_GetCameraID(&params.camera, &params.id);
    else
    {
        Picam_ConnectDemoCamera(PicamModel_Sophia2048BUV135, "XO30000923", &params.id);
        Picam_OpenCamera(&params.id, &params.camera);
        printf("No Camera Detected, Creating Demo Camera\n");
    }

    Picam_GetEnumerationString(PicamEnumeratedType_Model, params.id.model, &params.string);
    printf("%s", params.string);
    printf(" (SN:%s) [%s]\n", params.id.serial_number, params.id.sensor_name);
    Picam_DestroyString(params.string);
    Picam_GetParameterIntegerValue(params.camera, PicamParameter_ReadoutStride, &params.readoutstride);

    //initial values
    // params.exposure_time = 6000;
    // set_exposure_time(10000);
    // set_temp(-10);
    // set_analog_gain(2);
    // commit_params();
    return OK;

}

int close_camera()
{
    // PicamHandle camera;
    std::cout << "Close camera" << std::endl;
    Picam_CloseCamera(params.camera);
    Picam_UninitializeLibrary();
    return OK;
}

int commit_params()
{
    pibln committed;
    PicamError error = Picam_AreParametersCommitted(params.camera, &committed);
    if (error != PicamError_None || !committed)
    {
        const PicamParameter* failed_parameters;
        piint failed_parameter_count;
        error = Picam_CommitParameters(params.camera, &failed_parameters, &failed_parameter_count);
        if (error != PicamError_None)
        {
            std::cerr << "Failed to commit parameters. ";
            PrintError(error);
            return ERR;
        }
        if (failed_parameter_count > 0)
        {
            Picam_DestroyParameters(failed_parameters);
            return -2;  // Indicate that some parameters failed to commit
        }
    }
    
    // If no params.errors and parameters are committed
    return OK;  // Indicate success
}

//new:
int burst(int i) {
    for (int j = 1; j <= i; ++j) {
        // Use stringstream to construct the filename
        std::stringstream ss;
        ss << "exposure_file_" << j << ".raw";
        std::string filename = ss.str(); // Get the string from stringstream
        
        // Call the expose function with the constructed filename
        expose(filename.c_str());
    }
    return OK;
}
//end new


int dark(const char *filename)
{
    std::cout << "Take dark" << std::endl;
    // set_shutter(2); // Set shutter mode to closed
    commit_params();
    params.exposure_time = EXP_TIME;
    set_exposure_time(params.exposure_time);
    commit_params();
    image(filename, &params.exposure_time);
    // raw_to_fits(filename);


    return OK;
}


int bias(const char *filename)
{
    std::cout << "Take bias" << std::endl;

    // set_shutter(3); // Set shutter mode to open
    commit_params();
    set_exposure_time(100);
    std::cerr << "[DEBUG] params.exposure_time in bias(): " << params.exposure_time << std::endl;
    commit_params();
    image(filename, &params.exposure_time);
    // raw_to_fits(filename);

    return OK;
}

// int get_status()
// {
//     if 
// }


int image(const char *filename, piflt *exposure_time)
{
    //TODO: set exposure time outside of Picam_Acquire(), and then use that value in Picam_Acquire()
    set_exposure_time(*exposure_time);
    commit_params();
    std::cerr << "[DEBUG] params.exposure_time: " << params.exposure_time << std::endl;

    std::cout << "[DEBUG] - image() static_cast<piint>(*exposure_time)" << static_cast<piint>(*exposure_time) << std::endl;

    int timeout = (int)(params.exposure_time * NUM_EXPOSURES + 2000); 

    PicamError error = Picam_Acquire(params.camera, NUM_EXPOSURES, timeout, &params.data, &params.errors); //changed 6000ms to params.exptime
   if (error == PicamError_None)
    {
        std::cout << "Successfully took frame" << std::endl;
        std::cout << "Filename is:" << filename << std::endl;

        PrintData( (pibyte*)params.data.initial_readout, 2, params.readoutstride );
		params.pFile = fopen(filename, "wb" );
        //set status to 'reading out'

		if( params.pFile )
		{
			if( !fwrite(params.data.initial_readout, 1, (2*params.readoutstride), params.pFile ) )
				printf( "Data file not saved\n" );
			fclose( params.pFile );
            //else: set status to 'writing'

		}
    }
    else
    {
        std::cerr << "Failed: ";
        PrintError(error);
        return ERR;
    }
    return OK;
}

int expose(const char* filename)
{
    std::cout << "Take exposure" << std::endl;

    // set_shutter(1);
    commit_params();
    params.exposure_time = EXP_TIME;
    set_exposure_time(params.exposure_time );
    std::cerr << "[DEBUG] params.exposure_time in expose(): " << params.exposure_time << std::endl;
    commit_params();
    image(filename, &params.exposure_time);
    // raw_to_fits(filename);
    return OK;

}

//the next few functions are for handling the raw file generated by "expose()", "bias()", or "dark()"
//resize raw file to 2048x2048
int resize_raw(const char* filename) {
    // const int element_size = 2; // sizeof(uint16_t)
    int target_elements = WIDTH * HEIGHT;
    int target_bytes = target_elements * ELEMENT_SIZE;

    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Error opening file!" << std::endl;
        return -1;
    }

    int file_size = static_cast<int>(file.tellg());
    file.seekg(0, std::ios::beg);
    int num_elements = file_size / ELEMENT_SIZE;

    std::cout << "Original file size: " << file_size << " bytes" << std::endl;
    std::cout << "Target size: " << target_bytes << " bytes" << std::endl;

    char* data = (char*)std::malloc(file_size);
    if (!data) {
        std::cerr << "Memory allocation failed!" << std::endl;
        return -1;
    }

    file.read(data, file_size);
    if (!file) {
        std::cerr << "Error reading the file!" << std::endl;
        std::free(data);
        return -1;
    }
    file.close();

    char* output_data = (char*)std::malloc(target_bytes);
    if (!output_data) {
        std::cerr << "Output memory allocation failed!" << std::endl;
        std::free(data);
        return -1;
    }

    if (num_elements < target_elements) {
        std::memcpy(output_data, data, num_elements * ELEMENT_SIZE);
        std::memset(output_data + num_elements * ELEMENT_SIZE, 0, (target_elements - num_elements) * ELEMENT_SIZE);
        std::cout << "Padded with " << (target_elements - num_elements) << " zeros" << std::endl;
    } else if (num_elements > target_elements) {
        std::memcpy(output_data, data, target_bytes);
        std::cout << "Trimmed extra data" << std::endl;
    } else {
        std::memcpy(output_data, data, target_bytes);
        std::cout << "No resizing needed." << std::endl;
    }

    std::free(data);
    return 0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////resize_raw

int raw_to_fits(const char *filename) {
// int raw_to_fits(const char *filename, piflt *temp, piflt *exposure_time, piint *gain) {

// piflt exposure_time;
if (resize_raw(filename) == 0){
    long naxes[2] = {WIDTH, HEIGHT};
    size_t npixels = WIDTH * HEIGHT;

    // fitsfile *fptr;      // FITS file pointer
    int status = 0;
    long fpixel = 1;

    // Allocate memory for the image
    unsigned short *image = (unsigned short *)malloc(npixels * sizeof(unsigned short));
    if (!image) {
        fprintf(stderr, "Memory allocation failed.\n");
        return 1;
    }

    // Open the raw binary file
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening raw file");
        free(image);
        return 1;
    }

    // Read raw image data
    if (fread(image, sizeof(unsigned short), npixels, fp) != npixels) {
        fprintf(stderr, "Error reading raw data.\n");
        fclose(fp);
        free(image);
        return 1;
    }
    fclose(fp);

    char fits_filename[256]; // Final output filename
    char filename_with_bang[256]; // Temp buffer to ensure filename starts with '!'

    // Add '!' at the beginning if not already there
    if (filename[0] != '!') {
        snprintf(filename_with_bang, sizeof(filename_with_bang), "!%s", filename);
    } else {
        strncpy(filename_with_bang, filename, sizeof(filename_with_bang));
        filename_with_bang[sizeof(filename_with_bang) - 1] = '\0'; // Ensure null termination
    }

    // Convert filename_with_bang -> fits_filename
    const char* ext = strrchr(filename_with_bang, '.');
    if (ext) {
        size_t basename_len = ext - filename_with_bang;
        if (basename_len >= sizeof(fits_filename) - 6) { // 5 for ".fits" + 1 null
            std::cerr << "Filename too long." << std::endl;
            return -1;
        }
        strncpy(fits_filename, filename_with_bang, basename_len);
        fits_filename[basename_len] = '\0';
        strcat(fits_filename, ".fits");
    } else {
        snprintf(fits_filename, sizeof(fits_filename), "%s.fits", filename_with_bang);
    }

    // char fits_path[260];
    // snprintf(fits_path, sizeof(fits_path), "!%s", fits_filename); 

    if (fits_create_file(&params.fptr, fits_filename, &status)) {
        fits_report_error(stderr, status);
        free(image);
        return status;
    }

    // Define image type and dimensions
    if (fits_create_img(params.fptr, SHORT_IMG, 2, naxes, &status)) {
        fits_report_error(stderr, status);
        fits_close_file(params.fptr, &status);
        free(image);
        return status;
    }

    // Write the image data
    if (fits_write_img(params.fptr, TUSHORT, fpixel, npixels, image, &status)) {
        fits_report_error(stderr, status);
    }
    // std::cerr << "[DEBUG] params.exposure_time before update_header - in raw_to_fits(): " << params.exposure_time << std::endl;

    update_header(&params.temp, &params.exposure_time, &params.gain);
 
    // Close the FITS file
    if (fits_close_file(params.fptr, &status)) {
        fits_report_error(stderr, status);
    }

    free(image);
    return status;
    }else{
        return -1;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////raw2fits

int update_header(piflt *temp, piflt *exposure_time, piint *gain){
// int update_header(){
    int status = 0;
    //update header

    if (fits_update_key(params.fptr, TDOUBLE, "TEMPERAT", temp,
                    "Temperature during exposure (C)", &status)) {
        fits_report_error(stderr, status);
    }

    // std::cerr << "[DEBUG] params.exposure_time before update_header - in update_header(): " << params.exposure_time << std::endl;
    if (fits_update_key(params.fptr, TDOUBLE, "EXPTIME", exposure_time,
                    "Exposure time (ms)", &status)) {
        fits_report_error(stderr, status);
    }

    if (fits_update_key(params.fptr, TINT, "GAIN", gain, //TFLOAT = 4.203895E-45 ?
                    "ADC analog gain", &status)) {
        fits_report_error(stderr, status);
    }

    char datetime_obs[30];
    time_t now = time(NULL);
    struct tm *utc_time = gmtime(&now);
    strftime(datetime_obs, sizeof(datetime_obs), "%Y-%m-%dT%H:%M:%S", utc_time);

    if (fits_update_key(params.fptr, TSTRING, "DATE-OBS", datetime_obs,
                        "Observation date and time (UTC)", &status)) {
        fits_report_error(stderr, status);
    }   

    //read out header values
    char comment[FLEN_COMMENT];
    float read_temp = 0.0;
    if (fits_read_key(params.fptr, TFLOAT, "TEMPERAT", &read_temp, comment, &status)) {
        fits_report_error(stderr, status);
    } else {
        std::cerr << "[CONFIRM] TEMPERAT from header: " << read_temp << " deg C" << std::endl;
    }

    float read_exptime = 0.0;
    if (fits_read_key(params.fptr, TFLOAT, "EXPTIME", &read_exptime, comment, &status)) {
        fits_report_error(stderr, status);
    } else {
        std::cerr << "[CONFIRM] EXPTIME from header: " << read_exptime << " ms" << std::endl;
    }

    int read_gain = 0.0;
    if (fits_read_key(params.fptr, TINT, "GAIN", &read_gain, comment, &status)) {
        fits_report_error(stderr, status);
    } else {
        std::cerr << "[CONFIRM] GAIN from header: " << read_gain << std::endl;
    }
    return status;
}

/////////////////////////////////////////////////////////////////////////////////////////////// update fits

// bool is_param_valid(){
//     time_t current_time = time(NULL);
//     return params.exposure_time_valid && 
//            (difftime(current_time, params.exposure_time_update) < PARAM_MAX_AGE_SECONDS);
// }

// void update_param_status() {
//     params.exposure_time_update = time(NULL);
//     params.exposure_time_valid = true;
// }

//orgin: FITS file originator
//object: //NAME OF OBJECT OBSERVED
//comment: fits file is used
//date
//date-obs
//exptime
//temp
//vacuum (from different source?)
//rois
//adcanaloggain
//rdnoise (hard coded in?)





// int main(){
//     open_camera();
//     burst(5);
//     // for (int i; i<2;++i)
//     // {
//     //     const char *file = "exp_file";
//     //     expose(file);
//     //     }
//     close_camera();
// }
