#include <iostream>  
#include <string>  
#include <vector>  
#include <libgen.h>  
#include <sys/stat.h>  
#include <sndfile.h>  
#include "spleeter/spleeter.h"  
  
void WriteWaveform(const spleeter::Waveform& data, const std::string& filename, int sample_rate) {  
    SF_INFO sfinfo;  
    sfinfo.channels = 2;  
    sfinfo.samplerate = sample_rate;  
    sfinfo.format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;  
      
    SNDFILE* outfile = sf_open(filename.c_str(), SFM_WRITE, &sfinfo);  
    if (!outfile) {  
        std::cerr << "Error creating output file: " << filename << "\n";  
        return;  
    }  
      
    // Convert Eigen matrix to interleaved format  
    std::vector<float> interleaved(data.size());  
    for (int frame = 0; frame < data.cols(); ++frame) {  
        interleaved[frame * 2] = data(0, frame);  
        interleaved[frame * 2 + 1] = data(1, frame);  
    }  
      
    sf_writef_float(outfile, interleaved.data(), data.cols());  
    sf_close(outfile);  
}  
  
int main(int argc, char* argv[]) {  
    if (argc != 3) {  
        std::cerr << "Usage: " << argv[0] << " <input.wav> <stem_type>\n";  
        std::cerr << "stem_type: 2stems, 4stems, or 5stems\n";  
        return 1;  
    }  
      
    std::string input_file = argv[1];  
    std::string stem_type = argv[2];  
      
    // Parse stem type  
    spleeter::SeparationType separation_type;  
    if (stem_type == "2stems") {  
        separation_type = spleeter::TwoStems;  
    } else if (stem_type == "4stems") {  
        separation_type = spleeter::FourStems;  
    } else if (stem_type == "5stems") {  
        separation_type = spleeter::FiveStems;  
    } else {  
        std::cerr << "Invalid stem type. Use: 2stems, 4stems, or 5stems\n";  
        return 1;  
    }  
      
    // Create output directory (C++14 compatible)  
    std::string input_basename = basename(const_cast<char*>(input_file.c_str()));  
    size_t dot_pos = input_basename.find_last_of('.');  
    if (dot_pos != std::string::npos) {  
        input_basename = input_basename.substr(0, dot_pos);  
    }  
    std::string output_dir = input_basename + "_stems";  
    mkdir(output_dir.c_str(), 0755);  
      
    // Initialize spleeter  
    std::error_code err;  
    spleeter::Initialize("./models/offline", {separation_type}, err);  
    if (err) {  
        std::cerr << "Failed to initialize spleeter: " << err.message() << "\n";  
        return 1;  
    }  
      
    // Read input file with libsndfile   
    SF_INFO sfinfo;  
    SNDFILE* infile = sf_open(input_file.c_str(), SFM_READ, &sfinfo);  
    if (!infile) {  
        std::cerr << "Error opening input file: " << sf_strerror(nullptr) << "\n";  
        return 1;  
    }  
      
    if (sfinfo.channels != 2 || sfinfo.samplerate != 44100) {  
        std::cerr << "Input must be 44.1kHz stereo\n";  
        sf_close(infile);  
        return 1;  
    }  
      
    std::vector<float> data(sfinfo.frames * sfinfo.channels);  
    sf_readf_float(infile, data.data(), sfinfo.frames);  
    sf_close(infile);  
      
    auto source = Eigen::Map<spleeter::Waveform>(data.data(), 2, sfinfo.frames);  
      
    // Perform separation and write outputs  
    if (stem_type == "2stems") {  
        spleeter::Waveform vocals, accompaniment;  
        spleeter::Split(source, &vocals, &accompaniment, err);  
        if (!err) {  
            WriteWaveform(vocals, output_dir + "/vocals.wav", sfinfo.samplerate);  
            WriteWaveform(accompaniment, output_dir + "/accompaniment.wav", sfinfo.samplerate);  
        }  
    } else if (stem_type == "4stems") {  
        spleeter::Waveform vocals, drums, bass, other;  
        spleeter::Split(source, &vocals, &drums, &bass, &other, err);  
        if (!err) {  
            WriteWaveform(vocals, output_dir + "/vocals.wav", sfinfo.samplerate);  
            WriteWaveform(drums, output_dir + "/drums.wav", sfinfo.samplerate);  
            WriteWaveform(bass, output_dir + "/bass.wav", sfinfo.samplerate);  
            WriteWaveform(other, output_dir + "/other.wav", sfinfo.samplerate);  
        }  
    } else if (stem_type == "5stems") {  
        spleeter::Waveform vocals, drums, bass, piano, other;  
        spleeter::Split(source, &vocals, &drums, &bass, &piano, &other, err);  
        if (!err) {  
            WriteWaveform(vocals, output_dir + "/vocals.wav", sfinfo.samplerate);  
            WriteWaveform(drums, output_dir + "/drums.wav", sfinfo.samplerate);  
            WriteWaveform(bass, output_dir + "/bass.wav", sfinfo.samplerate);  
            WriteWaveform(piano, output_dir + "/piano.wav", sfinfo.samplerate);  
            WriteWaveform(other, output_dir + "/other.wav", sfinfo.samplerate);  
        }  
    }  
      
    if (err) {  
        std::cerr << "Separation failed: " << err.message() << "\n";  
        return 1;  
    }  
      
    std::cout << "Separation complete. Output saved to: " << output_dir << "\n";  
    return 0;  
}
