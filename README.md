# What is this?
BTT's Knomi 2 supports adding a OV2640 camera.

[OmeeeChan](https://github.com/OmeeeChan/Knomi-V2-camera-function) took the effort to implement this camera into knomi's code rudimentally.

I wanted to make this a "round thing". My fork integrates the camera smoothly into the WebUI including instructions to use it with Mainsail and vieweing status codes for error handling.

# Where can I find the code?

Check out the firmware-fsedarkalex branch, open it with PlatformIO and flash it.
I might provide a precompiled firmware at some point through Releases (If I find out how that is working).

# Will this firmware run on Knomi (v1)?

Maybe yes, maybe no. I can't test it. The Knomi v1 has no Camera interface, so it would not make sense.
I am developing specifically for Knomi v2.

# Where is all the GIfs?

Here: https://github.com/bigtreetech/KNOMI/tree/master

# Is this up to date?

Maybe :)

I will update my fork as I make changes to knomi 2 code.

# I want feature XY or imprevement Z...

You can put it into issues but I am not aiming to create an alternative firmware. I only want to adjust knomi to my needs, keeping things universal.

# I am a representative of BTT. May we integrate your code changes back?

Yes. A reference to OmeeeChan and me somewhere in the code comments or credits files would be appreciated.