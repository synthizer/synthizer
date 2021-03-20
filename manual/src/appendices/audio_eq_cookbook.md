# Audio EQ Cookbook

The following is the Audio EQ Cookbook, containing the most widely used formulas for biquad filters.  Synthizer's
internal implementation of most filters either follows these exactly or is composed of cascaded/parallel sections.

There are several versions of this document on the web.
This version is from http://music.columbia.edu/pipermail/music-dsp/2001-March/041752.html.

```
         Cookbook formulae for audio EQ biquad filter coefficients
---------------------------------------------------------------------------
by Robert Bristow-Johnson <rbj at gisco.net>  a.k.a. <robert at audioheads.com>


All filter transfer functions were derived from analog prototypes (that 
are shown below for each EQ filter type) and had been digitized using the 
Bilinear Transform.  BLT frequency warping has been taken into account 
for both significant frequency relocation and for bandwidth readjustment.

First, given a biquad transfer function defined as:

            b0 + b1*z^-1 + b2*z^-2
    H(z) = ------------------------                                (Eq 1)
            a0 + a1*z^-1 + a2*z^-2

This shows 6 coefficients instead of 5 so, depending on your architechture,
you will likely normalize a0 to be 1 and perhaps also b0 to 1 (and collect
that into an overall gain coefficient).  Then your transfer function would
look like:

            (b0/a0) + (b1/a0)*z^-1 + (b2/a0)*z^-2
    H(z) = ---------------------------------------                 (Eq 2)
               1 + (a1/a0)*z^-1 + (a2/a0)*z^-2

or

                      1 + (b1/b0)*z^-1 + (b2/b0)*z^-2
    H(z) = (b0/a0) * ---------------------------------             (Eq 3)
                      1 + (a1/a0)*z^-1 + (a2/a0)*z^-2


The most straight forward implementation would be the Direct I form (using Eq 2):

y[n] = (b0/a0)*x[n] + (b1/a0)*x[n-1] + (b2/a0)*x[n-2]
                    - (a1/a0)*y[n-1] - (a2/a0)*y[n-2]              (Eq 4)

This is probably both the best and the easiest method to implement in the 56K.



Now, given:

    sampleRate (the sampling frequency)

    frequency ("wherever it's happenin', man."  "center" frequency 
        or "corner" (-3 dB) frequency, or shelf midpoint frequency, 
        depending on which filter type)
    
    dBgain (used only for peaking and shelving filters)

    bandwidth in octaves (between -3 dB frequencies for BPF and notch
        or between midpoint (dBgain/2) gain frequencies for peaking EQ)

     _or_ Q (the EE kind of definition)

     _or_ S, a "shelf slope" parameter (for shelving EQ only).  when S = 1, 
        the shelf slope is as steep as it can be and remain monotonically 
        increasing or decreasing gain with frequency.  the shelf slope, in 
        dB/octave, remains proportional to S for all other values.



First compute a few intermediate variables:

    A     = sqrt[ 10^(dBgain/20) ]
          = 10^(dBgain/40)                    (for peaking and shelving EQ filters only)

    omega = 2*PI*frequency/sampleRate

    sin   = sin(omega)
    cos   = cos(omega)

    alpha = sin/(2*Q)                                     (if Q is specified)
          = sin*sinh[ ln(2)/2 * bandwidth * omega/sin ]   (if bandwidth is specified)

    beta  = sqrt(A)/Q                                     (for shelving EQ filters only)
          = sqrt(A)*sqrt[ (A + 1/A)*(1/S - 1) + 2 ]       (if shelf slope is specified)
          = sqrt[ (A^2 + 1)/S - (A-1)^2 ]


Then compute the coefficients for whichever filter type you want:

  The analog prototypes are shown for normalized frequency.
  The bilinear transform substitutes:
  
                1          1 - z^-1
  s  <-  -------------- * ----------
          tan(omega/2)     1 + z^-1

and makes use of these trig identities:

                    sin(w)
   tan(w/2)    = ------------
                  1 + cos(w)


                  1 - cos(w)
  (tan(w/2))^2 = ------------
                  1 + cos(w)



LPF:            H(s) = 1 / (s^2 + s/Q + 1)

                b0 =  (1 - cos)/2
                b1 =   1 - cos
                b2 =  (1 - cos)/2
                a0 =   1 + alpha
                a1 =  -2*cos
                a2 =   1 - alpha



HPF:            H(s) = s^2 / (s^2 + s/Q + 1)

                b0 =  (1 + cos)/2
                b1 = -(1 + cos)
                b2 =  (1 + cos)/2
                a0 =   1 + alpha
                a1 =  -2*cos
                a2 =   1 - alpha



BPF (constant skirt gain):    H(s) = s / (s^2 + s/Q + 1)

                b0 =   Q*alpha
                b1 =   0
                b2 =  -Q*alpha
                a0 =   1 + alpha
                a1 =  -2*cos
                a2 =   1 - alpha


BPF (constant peak gain):     H(s) = (s/Q) / (s^2 + s/Q + 1)

                b0 =   alpha
                b1 =   0
                b2 =  -alpha
                a0 =   1 + alpha
                a1 =  -2*cos
                a2 =   1 - alpha



notch:          H(s) = (s^2 + 1) / (s^2 + s/Q + 1)

                b0 =   1
                b1 =  -2*cos
                b2 =   1
                a0 =   1 + alpha
                a1 =  -2*cos
                a2 =   1 - alpha



APF:          H(s) = (s^2 - s/Q + 1) / (s^2 + s/Q + 1)

                b0 =   1 - alpha
                b1 =  -2*cos
                b2 =   1 + alpha
                a0 =   1 + alpha
                a1 =  -2*cos
                a2 =   1 - alpha



peakingEQ:      H(s) = (s^2 + s*(A/Q) + 1) / (s^2 + s/(A*Q) + 1)

                b0 =   1 + alpha*A
                b1 =  -2*cos
                b2 =   1 - alpha*A
                a0 =   1 + alpha/A
                a1 =  -2*cos
                a2 =   1 - alpha/A



lowShelf:       H(s) = A * (s^2 + beta*s + A) / (A*s^2 + beta*s + 1)

                b0 =    A*[ (A+1) - (A-1)*cos + beta*sin ]
                b1 =  2*A*[ (A-1) - (A+1)*cos            ]
                b2 =    A*[ (A+1) - (A-1)*cos - beta*sin ]
                a0 =        (A+1) + (A-1)*cos + beta*sin
                a1 =   -2*[ (A-1) + (A+1)*cos            ]
                a2 =        (A+1) + (A-1)*cos - beta*sin



highShelf:      H(s) = A * (A*s^2 + beta*s + 1) / (s^2 + beta*s + A)

                b0 =    A*[ (A+1) + (A-1)*cos + beta*sin ]
                b1 = -2*A*[ (A-1) + (A+1)*cos            ]
                b2 =    A*[ (A+1) + (A-1)*cos - beta*sin ]
                a0 =        (A+1) - (A-1)*cos + beta*sin
                a1 =    2*[ (A-1) - (A+1)*cos            ]
                a2 =        (A+1) - (A-1)*cos - beta*sin

```