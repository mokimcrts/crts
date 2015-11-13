#ifndef _CE_SIMULTANEOUS_RX_AND_SENSING__
#define _CE_SIMULTANEOUS_RX_AND_SENSING_

#include <liquid/liquid.h>
#include "CE.hpp"
#include "timer.h"

class CE_Simultaneous_RX_And_Sensing : public Cognitive_Engine {
    
    private:
		// sensing parameters
		static const float sensing_delay_ms = 1e3;
		static const int fft_length = 512;
		static const int fft_averaging = 10;

		// timer to start and stop sensing
		timer t;
		long int sense_time_s;
		long int sense_time_us;

		// counter for fft averaging
		int fft_counter;
		
		// USRP, FFT output, and average FFT  sample buffers
		float _Complex buffer[fft_length];
		float _Complex buffer_F[fft_length]; 
		float fft_avg[fft_length];

		// fft plan for spectrum sensing
    	fftplan fft;
	public:
        CE_Simultaneous_RX_And_Sensing();
        ~CE_Simultaneous_RX_And_Sensing();
        virtual void execute(void * _args);
};

#endif