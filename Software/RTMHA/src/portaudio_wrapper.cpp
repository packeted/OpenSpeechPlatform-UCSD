#include "portaudio_wrapper.h"

#include <iostream>
#include <sstream>

#ifdef __linux__
#include <pa_linux_alsa.h>
#endif

#ifdef __APPLE__
#include <pa_mac_core.h>
#endif

static const PaSampleFormat SAMPLE_TYPE = paFloat32 | paNonInterleaved;


portaudio_wrapper::portaudio_wrapper(int in_device, int in_num_channel, int out_device, int out_num_channels,
                                     PaStreamCallback callback, void *userData, int sample_rate,
                                     unsigned long frames_per_buffer){
    samp_rate = sample_rate;
    this->frames_per_buffer = frames_per_buffer;
    err = Pa_Initialize();
    if( err != paNoError ) {
        this->delete_stream();
        return;
    }
    if(this->init_stream(in_device, in_num_channel, out_device, out_num_channels, callback, userData)){
        std::cout << "Error Setting Up PortAudio Stream" << std::endl;
    }
}
portaudio_wrapper::portaudio_wrapper(int in_num_channel, int out_num_channels, PaStreamCallback callback, void *userData,
                                     int sample_rate, unsigned long frames_per_buffer) {
    samp_rate = sample_rate;
    this->frames_per_buffer = frames_per_buffer;
    err = Pa_Initialize();
    if( err != paNoError ) {
        this->delete_stream();
        return;
    }
    if(this->init_stream(Pa_GetDefaultInputDevice(), in_num_channel, Pa_GetDefaultOutputDevice(), out_num_channels,
                         callback, userData)){
        std::cout << "Error Setting Up PortAudio Stream" << std::endl;
    }
}
portaudio_wrapper::~portaudio_wrapper() {
    this->delete_stream();
}
int portaudio_wrapper::init_stream(int in_device, int in_num_channels, int out_device, int out_num_channels,
                                    PaStreamCallback callback, void* userData) {

    /* -- setup -- */
    inputInfo = Pa_GetDeviceInfo( in_device );
    inputParameters.device = in_device; /* default input device */
    inputParameters.channelCount = in_num_channels;
    inputParameters.sampleFormat = SAMPLE_TYPE;
    inputParameters.suggestedLatency = inputInfo->defaultHighInputLatency ;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    outputInfo = Pa_GetDeviceInfo( out_device );
    outputParameters.device = out_device; /* default output device */
    outputParameters.channelCount = out_num_channels;
    outputParameters.sampleFormat = SAMPLE_TYPE;
    outputParameters.suggestedLatency =  outputInfo->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;

#ifdef __APPLE__
    PaMacCoreStreamInfo mac;
    PaMacCore_SetupStreamInfo(&mac, paMacCorePro);
    inputParameters.hostApiSpecificStreamInfo = (void*) &mac;
    outputParameters.hostApiSpecificStreamInfo = (void*) &mac;
#endif

    // Open stream
    if(in_num_channels == 0){
        err = Pa_OpenStream(
                &stream,
                nullptr,
                &outputParameters,
                samp_rate,
                frames_per_buffer,
                paClipOff|paDitherOff|paPrimeOutputBuffersUsingStreamCallback,      /* we won't output out of range samples so don't bother clipping them */
                callback, /* no callback, use blocking API */
                userData ); /* no callback, so no callback userData */
    }
    else{
        err = Pa_OpenStream(
                &stream,
                &inputParameters,
                &outputParameters,
                samp_rate,
                frames_per_buffer,
                paClipOff|paDitherOff|paPrimeOutputBuffersUsingStreamCallback,      /* we won't output out of range samples so don't bother clipping them */
                callback, /* no callback, use blocking API */
                userData ); /* no callback, so no callback userData */
    }


    if( err != paNoError ) {
        std::cerr << "Failed to create PortAudio stream" << std::endl;
        this->delete_stream();
        return -1;
    }

#ifdef __linux__
    PaAlsa_EnableRealtimeScheduling ( &stream, 1);
#endif

    // Print stats
    std::stringstream ss;
    ss << "Input device # " << inputParameters.device << std::endl;
    ss << "  Name: " << inputInfo->name << std::endl;
    ss << "  Channels = " << out_num_channels << std::endl;
    ss << "  LL: " << inputInfo->defaultLowInputLatency << " seconds"  << std::endl;
    ss << "  HL: " << inputInfo->defaultHighInputLatency << " seconds" << std::endl;
    ss << "Output device # " << outputParameters.device << std::endl;
    ss << "  Name: " << outputInfo->name << std::endl;
    ss << "  Channels = " << in_num_channels << std::endl;
    ss << "  LL: " << outputInfo->defaultLowOutputLatency << " seconds" << std::endl;
    ss << "  HL: " << outputInfo->defaultHighOutputLatency << " seconds" << std::endl;
    std::cout << ss.str() << std::endl;

    return 0;
}

void portaudio_wrapper::delete_stream() {

    if( stream ) {
        Pa_AbortStream( stream );
        Pa_CloseStream( stream );
    }
    Pa_Terminate();
    if( err & paInputOverflow )
        fprintf( stderr, "Input Overflow.\n" );
    if( err & paOutputUnderflow )
        fprintf( stderr, "Output Underflow.\n" );

}

int portaudio_wrapper::start_stream() {
    err = Pa_StartStream( stream );
    if( err != paNoError ) {
        delete_stream();
        return -1;
    }

    return 0;

}

int portaudio_wrapper::stop_stream() {
    err = Pa_StopStream( stream );
    if( err != paNoError ) {
        delete_stream();
        return -1;
    }
    return 0;

}

double portaudio_wrapper::input_latency() {
    auto si = Pa_GetStreamInfo(stream);
    return si->inputLatency;
}

double portaudio_wrapper::output_latency() {
    auto si = Pa_GetStreamInfo(stream);
    return si->outputLatency;
}
