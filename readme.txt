============================================================================================
I.Build general wrapper linux (Ubuntu x86_64)
============================================================================================
Please run below command:

sh build.sh

============================================================================================
II. Run sample codes
============================================================================================
1. Demo callback APIs. When users want to register a function callback for each stream or even an empty function.

$ sh run_callback.sh

2. Demo pipeline APIs. When each stream is ready insert to the working queue
for user to retrieve the frame.

$ sh run_pipeline.sh

3. Demo FramesetPipeline APIs. When color, depth, and point cloud are totally available insert to working queue
for user to retrieve the frame set. Its performance strongly depends on your platform.

$ sh run_frameset_pipeline.sh
