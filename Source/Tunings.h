#pragma once
#include <vector>
#include <cmath>

// Notas em numero MIDI (E2 = 40), da corda mais grave para a mais aguda
// (no ukulele a ordem e a das cordas: G C E A).
namespace Tunings
{
    struct Tuning
    {
        const char* name;
        std::vector<int> notes;
    };

    // instrument: 0 = cromatico, 1 = violao/guitarra, 2 = baixo 4, 3 = baixo 5, 4 = ukulele
    inline const std::vector<Tuning>& get (int instrument)
    {
        static const std::vector<Tuning> none;

        static const std::vector<Tuning> guitar = {
            { "Padrao (E A D G B E)",          { 40, 45, 50, 55, 59, 64 } },
            { "Drop D (D A D G B E)",          { 38, 45, 50, 55, 59, 64 } },
            { "Meio tom abaixo (Eb)",          { 39, 44, 49, 54, 58, 63 } },
            { "Um tom abaixo (D)",             { 38, 43, 48, 53, 57, 62 } },
            { "Drop C (C G C F A D)",          { 36, 43, 48, 53, 57, 62 } },
            { "DADGAD",                        { 38, 45, 50, 55, 57, 62 } },
            { "Open G (D G D G B D)",          { 38, 43, 50, 55, 59, 62 } },
            { "Open D (D A D F# A D)",         { 38, 45, 50, 54, 57, 62 } }
        };

        static const std::vector<Tuning> bass4 = {
            { "Padrao (E A D G)",              { 28, 33, 38, 43 } },
            { "Drop D (D A D G)",              { 26, 33, 38, 43 } },
            { "Meio tom abaixo (Eb)",          { 27, 32, 37, 42 } },
            { "D padrao (D G C F)",            { 26, 31, 36, 41 } },
            { "Drop C (C G C F)",              { 24, 31, 36, 41 } }
        };

        static const std::vector<Tuning> bass5 = {
            { "Padrao (B E A D G)",            { 23, 28, 33, 38, 43 } },
            { "Drop A (A E A D G)",            { 21, 28, 33, 38, 43 } },
            { "Meio tom abaixo (Bb)",          { 22, 27, 32, 37, 42 } }
        };

        static const std::vector<Tuning> ukulele = {
            { "Padrao (G C E A)",              { 67, 60, 64, 69 } },
            { "Baritono (D G B E)",            { 50, 55, 59, 64 } }
        };

        switch (instrument)
        {
            case 1:  return guitar;
            case 2:  return bass4;
            case 3:  return bass5;
            case 4:  return ukulele;
            default: return none;
        }
    }

    inline double midiToFreq (int midi, double refA)
    {
        return refA * std::pow (2.0, (midi - 69) / 12.0);
    }
}
