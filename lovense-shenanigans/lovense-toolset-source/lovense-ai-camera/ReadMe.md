## MediaPipe识别只支持64位

## 安装
插件安装目录：
C:\ProgramData\obs-studio\plugins\lovense-Ai-camera\bin\64bit  
安装文件：  
face_mesh_desktop_live.pbtxt  
Face_Tracking_dll.dll  
lovense-Ai-camera.dll  
opencv_world3410.dll  

资源安装目录：  
C:\Program Files\obs-studio\bin\64bit  
安装文件：  
包含：mediapipe目录所有文件  

## 在VS中调试插件
1. 将插件拷贝到安装目录，pdb也需要拷贝到安装目录；  
2. 在VS项目属性->调试选项设置OBS.exe启动路径，和OBS工作目录；  