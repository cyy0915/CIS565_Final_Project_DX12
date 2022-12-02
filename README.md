# Base code的Readme

This repository is intended to be used as a code repository for learning DirectX 12. The tutorials can be found on https://www.3dgep.com

This project uses [CMake](https://cmake.org/) (3.18.3 or newer) to generate the project and solution files.

To use this project, run the [GenerateProjectFiles.bat](GenerateProjectFiles.bat) script and open the generated Visual Studio solution file in the build_vs2017 or build_vs2019 folder that gets created (depending on the version of Visual Studio you have installed).  (cyy注：也可以自己用cmake)

Assets for the samples can be downloaded using the [DownloadAssets.bat](DownloadAssets.bat) batch file located in the root folder of this project.

For more instructions see [Getting Started](https://github.com/jpvanoosten/LearningDirectX12/wiki/Getting-Started).


# Readme
暂时用samples/tutorial3作basecode
在dx12lib中完成了一个ComputeShader类用作compute shader示例，该类可以视为cuda的一个kernel函数，该类输入一个颜色，输出一张这个颜色的texture，compute shader每一个thread为texture中一个像素点赋值。代码文件中的注释说明了一些用法