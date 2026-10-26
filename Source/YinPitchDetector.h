#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// Detector de pitch YIN (de Cheveigne & Kawahara, 2002).
// Faixa: ~25 Hz (baixo de 5 cordas afinado grave) ate ~1200 Hz.
class YinPitchDetector
{
public:
    struct Result
    {
        float frequency = -1.0f; // Hz, ou -1 se nao detectou
        float clarity   = 0.0f;  // 0..1 (quanto maior, mais confiavel)
    };

    void prepare (double sr)
    {
        sampleRate = sr;
        tauMin   = std::max (2, (int) (sr / maxFreq));
        tauMax   = (int) (sr / minFreq) + 1;
        window   = (int) (0.045 * sr);          // janela de integracao (~45 ms)
        required = window + tauMax + 1;
        diff.assign ((size_t) tauMax + 1, 0.0f);
        cmnd.assign ((size_t) tauMax + 1, 1.0f);
    }

    int getRequiredSamples() const { return required; }

    // x deve apontar para 'required' amostras (mono)
    Result detect (const float* x)
    {
        // 1) funcao de diferenca
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double sum = 0.0;
            for (int j = 0; j < window; ++j)
            {
                const double d = (double) x[j] - (double) x[j + tau];
                sum += d * d;
            }
            diff[(size_t) tau] = (float) sum;
        }

        // 2) diferenca media normalizada cumulativa
        cmnd[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            running += diff[(size_t) tau];
            cmnd[(size_t) tau] = running > 0.0 ? (float) (diff[(size_t) tau] * tau / running) : 1.0f;
        }

        // 3) primeiro vale abaixo do limiar
        int found = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold)
            {
                while (tau + 1 < tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                    ++tau;
                found = tau;
                break;
            }
        }

        Result r;
        if (found < 1 || found >= tauMax)
            return r;

        // 4) interpolacao parabolica
        const float s0 = cmnd[(size_t) found - 1];
        const float s1 = cmnd[(size_t) found];
        const float s2 = cmnd[(size_t) found + 1];
        const float denom = 2.0f * (s0 - 2.0f * s1 + s2);
        const float better = std::abs (denom) > 1e-9f ? (float) found + (s0 - s2) / denom
                                                      : (float) found;

        r.frequency = (float) (sampleRate / better);
        r.clarity   = 1.0f - s1;
        return r;
    }

private:
    double sampleRate = 44100.0;
    int tauMin = 2, tauMax = 100, window = 1000, required = 1200;
    const float threshold = 0.15f;
    const double minFreq = 25.0, maxFreq = 1200.0;
    std::vector<float> diff, cmnd;
};
