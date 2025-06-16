/*
    camera.cpp
    Sophia camera control for Superlotis

    This script defines camera commands, configuration, and file handling operations for the Sophia camera
    using the Picam API and CFITSIO.
*/

#include "stdio.h" // Standard C I/O header
#include "picam.h" // PI camera SDK header
#include "camera.h" // Project-specific camera definitions
#include <iostream> // C++ standard I/O streams
#include <sstream> // String stream utilities for string formatting
#include <sys/stat.h>  // For stat and mkdir
#include <cerrno>      // For errno variable
#include <cstring>     // For strerror, memcpy, memset
#include <fstream>     // For file stream input/output
#include <vector>      // For using vectors
#include <algorithm>   // For std::copy, std::fill, etc.
#include <cstdint>     // For fixed-width integer types
#include <iomanip>     // For formatted I/O
#include <string>      // For std::string
#include <cstdlib>     // For malloc, free, etc.
#include <cstdio>      // For C-style file operations (fopen, fwrite, fclose)
extern "C" {
    #include "fitsio.h" // CFITSIO library for FITS file operations (C linkage)
}

//------------------------
// Macro Definitions
//------------------------
#define NO_TIMEOUT  -1 // Constant to indicate no timeout
#define TIME_BETWEEN_READOUTS 10 // Milliseconds between readouts
#define NUM_EXPOSURES  2 // Number of exposures per acquisition
#define EXP_TIME 6000 // Default exposure time in milliseconds
#define ACQUIRE_TIMEOUT 15000  // Timeout for acquisition in milliseconds
#define OK 0 // Function return value for success
#define ERR -1 // Function return value for error
#define ELEMENT_SIZE 2 // Bytes per image element (for dtype = uint16)
#define WIDTH 2048 // Image width in pixels
#define HEIGHT 2048 // Image height in pixels

//------------------------
// Global Parameters
//------------------------
PicamPtcArgs params; // Global parameter structure for camera and acquisition state

//------------------------
// Utility Functions
//------------------------

/**
 * Prints the middle pixel value from each acquired frame for diagnostic purposes.
 * @param buf Pointer to raw image data.
 * @param numframes Number of frames in buffer.
 * @param framelength Length of each frame in bytes.
 */
void PrintData( pibyte* buf, piint numframes, piint framelength )
{
    pi16u  *midpt = NULL; // Pointer to midpoint of frame
    pibyte *frameptr = NULL; // Pointer to start of each frame

    for( piint loop = 0; loop < numframes; loop++ ) // Loop over each frame
    {
        frameptr = buf + ( framelength * loop ); // Point to start of this frame
        midpt = (pi16u*)frameptr + ( ( ( framelength/sizeof(pi16u) )/ 2 ) ); // Point to middle pixel
        printf( "%5d,%5d,%5d\t%d\n",(int) *(midpt-1),(int) *(midpt),(int) *(midpt+1),(int) loop+1 ); // Print value before, at, after middle pixel, and frame number
    }
}

/**
 * Prints a string representation of a Picam enumeration value.
 * @param type Enumeration type.
 * @param value Value to convert.
 * @return OK (0) on success, ERR (-1) on failure.
 */
int PrintEnumString(PicamEnumeratedType type, piint value)
{
    PicamError error = Picam_GetEnumerationString(type, value, &params.string); // Get enum string using Picam API
    if (error == PicamError_None) // Check if call succeeded
    {
        printf("%s", params.string); // Print string value
        Picam_DestroyString(params.string); // Free allocated string
        return OK; // Success
    }
    else
    {
        return ERR; // Failure
    }
}

/**
 * Prints a human-readable message for PicamError codes.
 * @param error The PicamError value.
 * @return OK for no error, ERR for error.
 */
int PrintError(PicamError error)
{
    if (error == PicamError_None){ // If no error
        printf("Succeeded\n"); // Print success
        return OK; // Success
    }
    else
    {
        printf("Failed ("); // Print error prefix
        PrintEnumString(PicamEnumeratedType_Error, error); // Print error string
        printf(")\n"); // Print error suffix
        return ERR; // Failure
    }
}

//------------------------
// Camera Parameter Control
//------------------------

/**
 * Gets the current camera exposure time.
 * @param exposure_time Pointer to store current exposure time (ms).
 * @return OK/ERR
 */
int get_exposure_time(piflt *exposure_time)
{
    printf("Getting current exposure time...\n"); // Inform user
    PicamError err = Picam_GetParameterFloatingPointValue(params.camera, PicamParameter_ExposureTime, &params.exposure_time); // Get exposure time value from camera

    if (err != PicamError_None) // If error
    {
        printf("Failed to get exposure time.\n"); // Inform user
        PrintError(err); // Print error message
        return ERR; // Failure
    }
    else
    {
        params.exposure_time = *exposure_time; // Store exposure time in global state
        return OK; // Success
    }
}

/**
 * Sets a new exposure time for the camera.
 * @param exposure_time New exposure time in ms.
 * @return OK/ERR
 */
int set_exposure_time(piflt exposure_time)
{
    params.exposure_time = exposure_time; // Save to global state

    printf("Setting new exposure time...\n"); // Inform user

    PicamError error = Picam_SetParameterFloatingPointValue(params.camera, PicamParameter_ExposureTime, exposure_time); // Set value on camera

    if (error != PicamError_None) // If error
    {
        printf("Failed to set exposure time.\n"); // Inform user
        PrintError(error); // Print error message
        return ERR; // Failure
    }
    else
    {
        return OK; // Success
    }
}

/**
 * Gets the current sensor temperature.
 * @param temp Pointer to store the temperature (Celsius).
 * @return OK/ERR
 */
int get_temp(piflt *temp)
{
    PicamError error; // Error holder
    std::cout << "Getting sensor temperature..."<< std::endl; // Inform user
    error = Picam_ReadParameterFloatingPointValue(
        params.camera, // Camera handle
        PicamParameter_SensorTemperatureReading, // Parameter to read
        temp); // Output pointer

    PrintError(error); // Print error if any
    if (error != PicamError_None) // If error
        return ERR; // Failure
    else
    {
        params.temp = *temp; // Store in global state
        std::cout << "*temp " << *temp << " degrees C" << std::endl; // Print temperature value
        std::cout << "params.temp " << params.temp << " degrees C" << std::endl; // Print global value
        return OK; // Success
    }
}

/**
 * Sets the sensor temperature, only if the temperature status is 'locked'.
 * @param temp New setpoint (Celsius).
 * @return OK on success, ERR on failure, -2 if unable to read status.
 */
int set_temp(piflt temp)
{
    PicamError error; // Error holder
    std::cout << "Checking sensor temperature status..." << std::endl; // Inform user
    PicamSensorTemperatureStatus status; // Status variable
    error = Picam_ReadParameterIntegerValue(
        params.camera, // Camera handle
        PicamParameter_SensorTemperatureStatus, // Parameter to read
        reinterpret_cast<piint*>(&status)); // Output pointer
    PrintError(error); // Print error message

    if (error == PicamError_None) // If status read succeeded
    {
        std::cout << "Status is: "; // Print prefix
        PrintEnumString(PicamEnumeratedType_SensorTemperatureStatus, status); // Print status string
        std::cout << std::endl; // Print newline

        if (status == PicamSensorTemperatureStatus_Locked) // If locked
        {
            std::cout << "Setting sensor temperature..." << std::endl; // Inform user
            error = Picam_SetParameterFloatingPointValue(
                params.camera, // Camera handle
                PicamParameter_SensorTemperatureSetPoint, // Parameter to set
                temp); // Value to set
            PrintError(error); // Print error message
            return OK; // Success
        }
        else
        {
            std::cout << "Temperature is not locked. Skipping setting temperature." << std::endl; // Inform user
            return ERR; // Failure
        }
    }
    std::cerr << "Failed to read sensor temperature status." << std::endl; // Inform user
    return -2; // Indicate read status failed
}

/**
 * Gets the current ADC analog gain.
 * @param gain Pointer to store the gain value.
 * @return OK/ERR
 */
int get_analog_gain(piint *gain)
{
    std::cout << "Getting adc analog gain..." << std::endl; // Inform user
    PicamError error = Picam_GetParameterIntegerValue(params.camera, PicamParameter_AdcAnalogGain, &params.gain); // Get gain from camera

    if (error == PicamError_None) // If success
    {
        const char* gainDescription = "unknown"; // Default description
        switch (*(int*)gain) { // Interpret value
            case 1: gainDescription = "low"; break; // Case 1: Low
            case 2: gainDescription = "medium"; break; // Case 2: Medium
            case 3: gainDescription = "high"; break; // Case 3: High
        }
        std::cout << "Current analog gain value: " << gainDescription << std::endl; // Print description
        return OK; // Success
    }
    else
    {
        std::cout << "Failed to get analog gain." << std::endl; // Inform user
        return ERR; // Failure
    }
}

/**
 * Sets the ADC analog gain.
 * @param gain Gain value (1=low, 2=medium, 3=high).
 * @return OK/ERR
 */
int set_analog_gain(piint gain)
{
    PicamError error; // Error holder
    get_analog_gain(&gain); // Get current gain (side effect: updates global state)
    printf("Setting adc analog gain...\n"); // Inform user

    error = Picam_SetParameterIntegerValue(params.camera, PicamParameter_AdcAnalogGain, params.gain); // Set gain

    if (error == PicamError_None) // If success
        return OK; // Success
    else
        return ERR; // Failure
}

//------------------------
// Camera Initialization
//------------------------

/**
 * Opens the first available camera, or creates a demo camera if none is found.
 * Initializes camera parameters.
 * @return OK/ERR
 */
int open_camera()
{
    Picam_InitializeLibrary(); // Initialize Picam library
    std::cout << "Open camera" << std::endl; // Inform user

    if (Picam_OpenFirstCamera(&params.camera) == PicamError_None) // Try to open real camera
        Picam_GetCameraID(&params.camera, &params.id); // Get camera ID
    else // If no real camera found
    {
        Picam_ConnectDemoCamera(PicamModel_Sophia2048BUV135, "XO30000923", &params.id); // Connect demo camera
        Picam_OpenCamera(&params.id, &params.camera); // Open demo camera
        printf("No Camera Detected, Creating Demo Camera\n"); // Inform user
    }

    Picam_GetEnumerationString(PicamEnumeratedType_Model, params.id.model, &params.string); // Get camera model name
    printf("%s", params.string); // Print model
    printf(" (SN:%s) [%s]\n", params.id.serial_number, params.id.sensor_name); // Print serial and sensor name
    Picam_DestroyString(params.string); // Free model string
    Picam_GetParameterIntegerValue(params.camera, PicamParameter_ReadoutStride, &params.readoutstride); // Get readout stride
    return OK; // Success
}

/**
 * Closes the camera and uninitializes the library.
 * @return OK
 */
int close_camera()
{
    std::cout << "Close camera" << std::endl; // Inform user
    Picam_CloseCamera(params.camera); // Close camera handle
    Picam_UninitializeLibrary(); // Uninitialize library
    return OK; // Success
}

/**
 * Commits any pending parameter changes to the camera.
 * @return OK on success, ERR on failure, -2 if some parameters failed to commit.
 */
int commit_params()
{
    pibln committed; // Boolean: are parameters committed?
    PicamError error = Picam_AreParametersCommitted(params.camera, &committed); // Check if committed
    if (error != PicamError_None || !committed) // If not committed
    {
        const PicamParameter* failed_parameters; // Pointer to failed parameters
        piint failed_parameter_count; // Number of failed parameters
        error = Picam_CommitParameters(params.camera, &failed_parameters, &failed_parameter_count); // Commit parameters
        if (error != PicamError_None) // If error
        {
            std::cerr << "Failed to commit parameters. "; // Inform user
            PrintError(error); // Print error
            return ERR; // Failure
        }
        if (failed_parameter_count > 0) // If some parameters failed
        {
            Picam_DestroyParameters(failed_parameters); // Free list
            return -2; // Some failures
        }
    }
    return OK; // Success
}

//------------------------
// Image Acquisition
//------------------------

/**
 * Takes a burst of exposures, saving each to a separate file.
 * @param i Number of exposures to take.
 * @return OK
 */
int burst(int i) {
    for (int j = 1; j <= i; ++j) { // Loop from 1 to i
        std::stringstream ss; // String stream for filename
        ss << "exposure_file_" << j << ".raw"; // Build filename
        std::string filename = ss.str(); // Get filename string
        expose(filename.c_str()); // Take and save exposure
    }
    return OK; // Success
}

/**
 * Takes a dark frame (shutter closed, default exposure).
 * @param filename Output file name.
 * @return OK
 */
int dark(const char *filename)
{
    std::cout << "Take dark" << std::endl; // Inform user
    commit_params(); // Ensure parameters are up to date
    params.exposure_time = EXP_TIME; // Set default exposure
    set_exposure_time(params.exposure_time); // Set exposure time on camera
    commit_params(); // Commit changes
    image(filename, &params.exposure_time); // Acquire and save image
    return OK; // Success
}

/**
 * Takes a bias frame (short exposure).
 * @param filename Output file name.
 * @return OK
 */
int bias(const char *filename)
{
    std::cout << "Take bias" << std::endl; // Inform user
    commit_params(); // Ensure parameters are up to date
    set_exposure_time(100); // Set short exposure
    std::cerr << "[DEBUG] params.exposure_time in bias(): " << params.exposure_time << std::endl; // Debug print
    commit_params(); // Commit changes
    image(filename, &params.exposure_time); // Acquire and save image
    return OK; // Success
}

/**
 * Acquires an image and writes its raw data to a file.
 * @param filename Output file name.
 * @param exposure_time Exposure time in ms.
 * @return OK/ERR
 */
int image(const char *filename, piflt *exposure_time)
{
    set_exposure_time(*exposure_time); // Set exposure time on camera
    commit_params(); // Commit parameters
    std::cerr << "[DEBUG] params.exposure_time: " << params.exposure_time << std::endl; // Debug print
    std::cout << "[DEBUG] - image() static_cast<piint>(*exposure_time)" << static_cast<piint>(*exposure_time) << std::endl; // Debug print

    int timeout = (int)(params.exposure_time * NUM_EXPOSURES + 2000); // Calculate timeout

    PicamError error = Picam_Acquire(params.camera, NUM_EXPOSURES, timeout, &params.data, &params.errors); // Acquire image
    if (error == PicamError_None) // If success
    {
        std::cout << "Successfully took frame" << std::endl; // Inform user
        std::cout << "Filename is:" << filename << std::endl; // Print filename

        PrintData( (pibyte*)params.data.initial_readout, 2, params.readoutstride ); // Print data for diagnostics
        params.pFile = fopen(filename, "wb" ); // Open file for writing
        if( params.pFile ) // If file opened
        {
            if( !fwrite(params.data.initial_readout, 1, (2*params.readoutstride), params.pFile ) ) // Write image data
                printf( "Data file not saved\n" ); // Inform if write failed
            fclose( params.pFile ); // Close file
        }
    }
    else
    {
        std::cerr << "Failed: "; // Inform user
        PrintError(error); // Print error string
        return ERR; // Failure
    }
    return OK; // Success
}

/**
 * Main exposure routine: sets exposure, commits params, and calls image().
 * @param filename Output file name.
 * @return OK/ERR
 */
int expose(const char* filename)
{
    std::cout << "Take exposure" << std::endl; // Inform user
    commit_params(); // Ensure parameters are up to date
    params.exposure_time = EXP_TIME; // Set default exposure time
    set_exposure_time(params.exposure_time ); // Set on camera
    std::cerr << "[DEBUG] params.exposure_time in expose(): " << params.exposure_time << std::endl; // Debug print
    commit_params(); // Commit changes
    image(filename, &params.exposure_time); // Acquire and save image
    return OK; // Success
}

//------------------------
// Raw File Handling
//------------------------

/**
 * Resizes a raw file to 2048x2048 pixels (pads or trims as needed).
 * @param filename Name of the raw file to resize.
 * @return 0 on success, -1 on error.
 */
int resize_raw(const char* filename) {
    int target_elements = WIDTH * HEIGHT; // Number of pixels required
    int target_bytes = target_elements * ELEMENT_SIZE; // Target file size in bytes

    std::ifstream file(filename, std::ios::binary | std::ios::ate); // Open file at end for size
    if (!file) { // If open failed
        std::cerr << "Error opening file!" << std::endl; // Inform user
        return -1; // Failure
    }

    int file_size = static_cast<int>(file.tellg()); // Get file size
    file.seekg(0, std::ios::beg); // Seek to beginning
    int num_elements = file_size / ELEMENT_SIZE; // Compute number of elements

    std::cout << "Original file size: " << file_size << " bytes" << std::endl; // Print original size
    std::cout << "Target size: " << target_bytes << " bytes" << std::endl; // Print target size

    char* data = (char*)std::malloc(file_size); // Allocate buffer for file
    if (!data) { // If allocation failed
        std::cerr << "Memory allocation failed!" << std::endl; // Inform user
        return -1; // Failure
    }

    file.read(data, file_size); // Read file into buffer
    if (!file) { // If read failed
        std::cerr << "Error reading the file!" << std::endl; // Inform user
        std::free(data); // Free memory
        return -1; // Failure
    }
    file.close(); // Close file

    char* output_data = (char*)std::malloc(target_bytes); // Allocate buffer for output
    if (!output_data) { // If allocation failed
        std::cerr << "Output memory allocation failed!" << std::endl; // Inform user
        std::free(data); // Free input buffer
        return -1; // Failure
    }

    if (num_elements < target_elements) { // File too short, pad with zeros
        std::memcpy(output_data, data, num_elements * ELEMENT_SIZE); // Copy existing data
        std::memset(output_data + num_elements * ELEMENT_SIZE, 0, (target_elements - num_elements) * ELEMENT_SIZE); // Pad with zeros
        std::cout << "Padded with " << (target_elements - num_elements) << " zeros" << std::endl; // Inform user
    } else if (num_elements > target_elements) { // File too long, trim
        std::memcpy(output_data, data, target_bytes); // Copy only needed bytes
        std::cout << "Trimmed extra data" << std::endl; // Inform user
    } else { // Exact size
        std::memcpy(output_data, data, target_bytes); // Copy data
        std::cout << "No resizing needed." << std::endl; // Inform user
    }

    std::free(data); // Free input buffer
    return 0; // Success
}

//------------------------
// FITS Conversion and Header
//------------------------

/**
 * Converts a raw file to a FITS file, resizing as needed and updating header.
 * @param filename Raw file to convert (will become .fits).
 * @return 0 on success, nonzero on error.
 */
int raw_to_fits(const char *filename) {
    if (resize_raw(filename) == 0){ // Resize raw file
        long naxes[2] = {WIDTH, HEIGHT}; // Image dimensions
        size_t npixels = WIDTH * HEIGHT; // Total pixels
        int status = 0; // FITS status code
        long fpixel = 1; // First pixel index (FITS convention)

        unsigned short *image = (unsigned short *)malloc(npixels * sizeof(unsigned short)); // Allocate image buffer
        if (!image) { // If allocation failed
            fprintf(stderr, "Memory allocation failed.\n"); // Inform user
            return 1; // Failure
        }

        FILE *fp = fopen(filename, "rb"); // Open raw file
        if (!fp) { // If open failed
            perror("Error opening raw file"); // Inform user
            free(image); // Free buffer
            return 1; // Failure
        }

        if (fread(image, sizeof(unsigned short), npixels, fp) != npixels) { // Read image data
            fprintf(stderr, "Error reading raw data.\n"); // Inform user
            fclose(fp); // Close file
            free(image); // Free buffer
            return 1; // Failure
        }
        fclose(fp); // Close file

        char fits_filename[256]; // FITS filename buffer
        char filename_with_bang[256]; // For FITSIO: force file overwrite

        if (filename[0] != '!') { // If not already forced
            snprintf(filename_with_bang, sizeof(filename_with_bang), "!%s", filename); // Add '!'
        } else {
            strncpy(filename_with_bang, filename, sizeof(filename_with_bang)); // Copy as is
            filename_with_bang[sizeof(filename_with_bang) - 1] = '\0'; // Null terminate
        }

        const char* ext = strrchr(filename_with_bang, '.'); // Find file extension
        if (ext) {
            size_t basename_len = ext - filename_with_bang; // Compute base name length
            if (basename_len >= sizeof(fits_filename) - 6) { // Check for overrun
                std::cerr << "Filename too long." << std::endl; // Inform user
                return -1; // Failure
            }
            strncpy(fits_filename, filename_with_bang, basename_len); // Copy base name
            fits_filename[basename_len] = '\0'; // Null terminate
            strcat(fits_filename, ".fits"); // Add .fits extension
        } else {
            snprintf(fits_filename, sizeof(fits_filename), "%s.fits", filename_with_bang); // Append .fits
        }

        if (fits_create_file(&params.fptr, fits_filename, &status)) { // Create FITS file
            fits_report_error(stderr, status); // Print error
            free(image); // Free image buffer
            return status; // Return error
        }

        if (fits_create_img(params.fptr, SHORT_IMG, 2, naxes, &status)) { // Create image extension
            fits_report_error(stderr, status); // Print error
            fits_close_file(params.fptr, &status); // Close FITS file
            free(image); // Free buffer
            return status; // Return error
        }

        if (fits_write_img(params.fptr, TUSHORT, fpixel, npixels, image, &status)) { // Write image data
            fits_report_error(stderr, status); // Print error
        }

        update_header(&params.temp, &params.exposure_time, &params.gain); // Update FITS header

        if (fits_close_file(params.fptr, &status)) { // Close FITS file
            fits_report_error(stderr, status); // Print error
        }

        free(image); // Free image buffer
        return status; // Return status
    } else {
        return -1; // Failure
    }
}

/**
 * Updates FITS header with temperature, exposure, gain, and date-obs.
 * @param temp Pointer to temperature value.
 * @param exposure_time Pointer to exposure time.
 * @param gain Pointer to gain value.
 * @return FITS status code.
 */
int update_header(piflt *temp, piflt *exposure_time, piint *gain){
    int status = 0; // FITS status

    if (fits_update_key(params.fptr, TDOUBLE, "TEMPERAT", temp,
                    "Temperature during exposure (C)", &status)) { // Update temperature
        fits_report_error(stderr, status); // Print error
    }
    if (fits_update_key(params.fptr, TDOUBLE, "EXPTIME", exposure_time,
                    "Exposure time (ms)", &status)) { // Update exposure time
        fits_report_error(stderr, status); // Print error
    }
    if (fits_update_key(params.fptr, TINT, "GAIN", gain,
                    "ADC analog gain", &status)) { // Update gain
        fits_report_error(stderr, status); // Print error
    }

    char datetime_obs[30]; // Buffer for observation date/time string
    time_t now = time(NULL); // Get current time
    struct tm *utc_time = gmtime(&now); // Convert to UTC
    strftime(datetime_obs, sizeof(datetime_obs), "%Y-%m-%dT%H:%M:%S", utc_time); // Format as ISO string

    if (fits_update_key(params.fptr, TSTRING, "DATE-OBS", datetime_obs,
                        "Observation date and time (UTC)", &status)) { // Update date-obs
        fits_report_error(stderr, status); // Print error
    }   

    char comment[FLEN_COMMENT]; // Buffer for FITS comment strings
    float read_temp = 0.0; // Variable for reading header value
    if (fits_read_key(params.fptr, TFLOAT, "TEMPERAT", &read_temp, comment, &status)) { // Read back temperature
        fits_report_error(stderr, status); // Print error
    } else {
        std::cerr << "[CONFIRM] TEMPERAT from header: " << read_temp << " deg C" << std::endl; // Print confirmation
    }

    float read_exptime = 0.0; // Variable for reading header value
    if (fits_read_key(params.fptr, TFLOAT, "EXPTIME", &read_exptime, comment, &status)) { // Read back exposure time
        fits_report_error(stderr, status); // Print error
    } else {
        std::cerr << "[CONFIRM] EXPTIME from header: " << read_exptime << " ms" << std::endl; // Print confirmation
    }

    int read_gain = 0.0; // Variable for reading header value
    if (fits_read_key(params.fptr, TINT, "GAIN", &read_gain, comment, &status)) { // Read back gain
        fits_report_error(stderr, status); // Print error
    } else {
        std::cerr << "[CONFIRM] GAIN from header: " << read_gain << std::endl; // Print confirmation
    }
    return status; // Return FITS status
}

//------------------------
// End of camera.cpp
//------------------------
