# Overview
This is a GPU based path tracer. The project focus on improving performance of the path tracer using screen space tracing, ray marching with SDF, and radiance cache.

# Features
### `Screen Space Tracing`

placeholder

### `Ray Marching with SDF`
The rendering below is run on our [experiment CUDA path tracer project](https://github.com/linlinbest/SDFPathTracer).
Ray Marching with SDF (17.6 FPS)  |  Ray Tracing  (4 FPS)
:-------------------------:|:-------------------------:
![](img/sdf/sdf1.PNG)        |  ![](img/sdf/rayTracing1.PNG)

As we can see from the image, the performance improvement is huge. Ray marching with SDF is roughly 4 times faster than ray tracing method. Even if there are more geometries in the scene, ray marching with SDF still maintains stable FPS, while FPS would drop with ray tracing.
However, ray marching with SDF can cause rendering in some parts of the scene inaccurate.

Ray marching with low resolution SDF voxels
![](img/sdf/sdf2.PNG)

### `Radiance Cache`

placeholder


# Readme from Our Base Code

This repository is intended to be used as a code repository for learning DirectX 12. The tutorials can be found on https://www.3dgep.com

This project uses [CMake](https://cmake.org/) (3.18.3 or newer) to generate the project and solution files.

To use this project, run the [GenerateProjectFiles.bat](GenerateProjectFiles.bat) script and open the generated Visual Studio solution file in the build_vs2017 or build_vs2019 folder that gets created (depending on the version of Visual Studio you have installed).

Assets for the samples can be downloaded using the [DownloadAssets.bat](DownloadAssets.bat) batch file located in the root folder of this project.

For more instructions see [Getting Started](https://github.com/jpvanoosten/LearningDirectX12/wiki/Getting-Started).