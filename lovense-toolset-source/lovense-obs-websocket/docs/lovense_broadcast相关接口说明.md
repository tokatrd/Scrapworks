<center><font face="黑体" size=5>lovense_broadcast相关接口说明v_2.0</font></center>

|  版本   | 修改内容  | 修改人 | 修改时间 |
|  ----  |   ----   | ----  | ----  |
| 1.1  |   增加mac webbluetooth说明  | wystan | 2021-09-29  |
| 1.2  |   增加显示隐藏OBS协议  | lcf | 2021-10-22  |
| 1.3  |   新增实时获取OBS隐藏状态的协议  | lcf | 2021-10-26 |
| 1.4  |   修改场景创建协议，新增特殊场景创建类型  | lcf | 2021-11-05 |
| 1.5  |   新增OBS广播协议，当OBS隐藏或显示、当OBS场景变化时  | lcf | 2021-11-05 |
| 1.6  |   添加webConnectService说明  | wystan | 2021-11-05 |
| 1.7  |   添加升级更新说明  | wystan | 2022-01-11 |
| 1.8  |   添加使用websocket激活用户摄像头  | lcf | 2022-02-21 |
| 1.9  |   添加获取OBS画面协议  | lcf | 2022-06-24 |
| 2.0  |   浏览器可以获取pcid  | lcf | 2022-09-15|
| 2.1  |   浏览器可以存储用户email等信息  | lcf | 2022-10-13|
| 2.2  |   插件更新完成通知  | oulong | 2023-02-14|

# lovense-browser:
    以下api需在extension的maninfest.json文件里面添加权限"lovense",同时最好都先判断是否是lovense-browser: if("lovense" in chrome)

+ **启动obs**
```js
chrome.lovense.open("obs",(ret)=>{
	console.log('open obs ret:' + ret);
});
```
+ **启动pc connect** //测试用,pc connect集成后不再支持
```js
chrome.lovense.open("pc_connect",(ret)=>{
	console.log('open pc_connect ret:' + ret);
});
```
+ **升级更新**
```js
chrome.lovense.getVersion（(res)=>{ localVer = res}）//获取本地版本

$.ajax({
       url: https://test2.lovense.com/app/getUpdate/fourInOne/windows?v=localVer.gver,
       type: 'post',
	   success(res) {
           	hasUpdate = res.hasUpdate;
           	updateUrl = res.url;
           }
	   }）//请求服务器是否需要升级
		
chrome.lovense.upgrade(updateUrl,(ret)=>{
	console.log('upgrade ret:' + ret);
});//如果需要升级，启动升级
```

+ **获取PCID**  
获取本机唯一标识符，pcid与105协议中上报的pcid一致
```js
chrome.lovense.getPcid((ret)=>{ 
	console.log('pcid:' + ret);
 });  
 返回值例如：{"pcid": "c9af29e1d75eba6adadcb47ebd944265"}
```

+ **存储email用户信息**  
获取本机唯一标识符，pcid与105协议中上报的pcid一致
```js
json格式如：
{
	"email":"1231231@qq.com",
	key:value,
}
chrome.lovense.saveConfigData(json,(ret)=>{ 
	console.log(ret);
 });  
 返回值例如："save data successd!"
```


---
---


# web玩具控制:

+ ***webConnectService方式***（ps:统一支持hid/serial dongle和mac bluetooth，在打包的webConnect_demo页面里面有此文件(webConnectService.js)的使用例子）

1: 初始化

```js
webConnectService.init(type,writeCallback,connectEventListener);
//type说明  0:hid dongle , 1:serail dongle , 2:mac bluetooth
function writeCallback(res){ //发送指令数据的回调，例如发送搜索指令后，这里就会收到搜到的玩具信息
  console.log('writeCallback:' + res)
}
function connectEventListener(event){//dongle 事件的回调，dongle打开失败还是成功
  console.log('connectEventListener:' + event)
}
```
2: 打开关闭dongle/bluetooth

```js
webConnectService.open();
webConnectService.close();
```
3: 发送控制指令(json格式指令)
```js
let searchCmd  = '{\"type\":\"usb\",\"func\":\"search\"}';
let vibrateCmd = '{\"type\":\"toy\",\"func\":\"command\",\"id\":\"' + toyId + '+\",\"cmd\":\"Vibrate:1\"}';
let connectCmd = '{\"type\":\"toy\",\"func\":\"connect\",\"id\":\"' + toyId + '\",\"eager\":1}';

webConnectService.sendCmd(searchCmd);//搜索玩具，发送指令后，如果dongle/bluetooth搜到玩具，就会通过writeCallback返回玩具信息
webConnectService.sendCmd(connectCmd);//连接搜索到的玩具
webConnectService.sendCmd(vibrateCmd);//震动已连接的玩具
```

+ ***dongle hid方式***

1: 获取dongle

```js
let hidDevice;
navigator.hid.getDevices().then(devices => {
	for(let i = 0; i < devices.length; i++){
		if('Lovense' == devices[i].productName){
			hidDevice = devices[i];
			break;
		}
	}
});
```
2: 连接dongle
```js
if(device){
	await device.open();
	device.addEventListener("inputreport", event => {});
}
```
3: 发送控制指令

```js
let searchCmd  = '{\"type\":\"usb\",\"func\":\"search\"}';//搜索玩具
sendCmd(searchCmd);
		
function sendCmd(strData){
	if (!device || strData.length == 0)
		return;

	let outputReport = Encodeuint8arr(strData + '\r\n')
	let piceData;
	let nTimes = parseInt(outputReport.length/64);
	for(let i = 0; i < nTimes;i++){
		piceData = outputReport.slice(i*64,(i+1)*64)
		device.sendReport(outputReportId, piceData).then(() => {}); 
	}

	piceData = outputReport.slice(nTimes*64,(nTimes + 1)*64)
	device.sendReport(outputReportId, piceData).then(() => {});
}
```

+ ***bluetooth方式***（mac）

1：扫描玩具
```js
const scan = await navigator.bluetooth.requestLEScan({
  acceptAllAdvertisements: true,
  keepRepeatedDevices: true
}) //同步等待30S后返回
```
2：注册蓝牙广播监听（扫描玩具返回后就会收到广播）
```js
var devices = []；//存放扫描到的玩具
var uuids = []；//存放玩具对应的uuid
navigator.bluetooth.addEventListener('advertisementreceived', event => {
    if(event.name == 'LVS-Lush143'){//可自由设置过滤条件，但是只有我们的玩具才有配对权限可以进行后续操作
    	devices.push(event.device)；
    	uuids.push(event.uuids[0])；
    }
});
```
3: 连接、操作玩具
```js
const replaceStr1 = (str, index, char) => {
  const strAry = str.split('');
  strAry[index] = char;
  return strAry.join('');
}

devices[0].gatt.connect().then(server => {//连接玩具//从广播收到的玩具里面选取一个玩具进行连接（注:不是我们玩具的其他蓝牙设备没有这个权限）
		return server.getPrimaryService(uuids[0]);
}).then(service => {//设置写指令服务
		return  service.getCharacteristic(replaceStr1(uuids[0],7,'2'));//写通道是固定第七位为2，读通道是固定第七位为3.
}).then(characteristic => {//发送操作指令
		characteristic.writeValue(new Encodeuint8arr('Vibrate:1')).then(() => {//发送震动指令
	});
}).catch((err)=>{
		  console.log(err);
});
```

---
---


# obs 控制(暂定用websocket):
+ **连接obs**
```js
let obs_ws = new WebSocket('ws://localhost:20896');
```
+ **获取scene列表**
```js
let msg_data = {
    request_type:'GetSceneList',
    message_id: '1358'
};

obs_ws.send(JSON.stringify(msg_data));
```
+ **设置当前scene**
```js
let msg_data = {
    request_type:'SetCurrentScene',
    message_id: '1358',
    scene-name: 'Scene 1'
};

obs_ws.send(JSON.stringify(msg_data));
```
+ **获取当前scene**
```js
let msg_data = {
    request_type:'GetCurrentScene',
    message_id: '1358'
};

obs_ws.send(JSON.stringify(msg_data));
```
+ **创建新scene**
```js
let msg_data = {
    request_type:'CreateScene',
    message_id: '1358',
    sceneName: 'My New Sence',
    sceneType: 0
};

参数说明：
sceneName：可以填空字符串，由OBS自动生成场景名字；
sceneType:{ //目前只支持以下值，其他值则默认生成空白场景
  0，//表示创建空白场景，默认生成名字为：Blank Scene
  1，//表示创建“Start soon”场景，默认生成名字为：Start soon
  2，//表示创建"Streaming style 01"场景，默认生成名字为：Streaming style 01
  3，//表示创建"Streaming style 02"场景，默认生成名字为：Streaming style 02
  4，//表示创建"Be right back"场景，默认生成名字为：Be right back
}

obs_ws.send(JSON.stringify(msg_data));
```
+ **设置rtmp信息**//不推荐rtmp推流
```js
let msg_data = {
    request_type:'SetStreamSettings',
    message_id: '1358',
    settings:{
      key: '47316563.b479e09303384babf9691e4cd28781658b2fbdf7ca8f72bafa904107e13df',
      server: 'rtmp://live.stream.highwebmedia.com/live-origin',
      service: 'chaturbate',
      type:"rtmp_common",
      save:true
    }
};

obs_ws.send(JSON.stringify(msg_data));
```
+ **启动rtmp推流**//不推荐rtmp推流
```js
let msg_data = {
    request_type:'StartStreaming',
    message_id: '1358'
};
成功返回：{"message_id":"1358","status":"ok"}
失败返回：{"error":"streaming connect error","message_id":"1358","status":"error"}
obs_ws.send(JSON.stringify(msg_data));
```
+ **停止rtmp推流**//不推荐rtmp推流
```js
let msg_data = {
    request_type:'StopStreaming',
    message_id: '1358'
};

obs_ws.send(JSON.stringify(msg_data));
```


+ **显示隐藏OBS** 
```js
let msg_data = {
    request_type:'ShowOBSApplication',
    message_id: '1358',
    show:true //显示为true,隐藏为false
};

obs_ws.send(JSON.stringify(msg_data));
```
+ **获取OBS是否显示的状态** 
```js
let msg_data = {
    request_type:'GetOBSApplicationShowStatus',
    message_id: '1358'
};
返回：{"message_id":"1358","show":false,"status":"ok"}
字段：show：true表示OBS处于显示状态；show:false表示OBS处于隐藏状态
obs_ws.send(JSON.stringify(msg_data));
```

## OBS 事件触发时，通过websocket广播协议

+ **当OBS显示或者隐藏时，触发协议** 
```js
//由websocket server对外广播
let broadcast_data = {
    update_type:'OBSApplicationShowChanged',
    show: true
};

example data:
{"show":true,"update_type":"OBSApplicationShowChanged"}
字段：show：true表示OBS处于显示状态；show:false表示OBS处于隐藏状态
```

+ **当OBS中的场景变化或者场景中的源变化时触发协议** 
```js
//由websocket server对外广播
let broadcast_data = {
    update_type:'ScenesChanged',
    scenes: [scenes items] //表示所有场景的数据
};
触发条件：
1. 切换场景时；
2. 添加、删除场景时；
3. 在场景中添加、删除源时；
4. 场景中的源排序发生变化时；

example data:
{
  "update_type": "ScenesChanged",
	"scenes": [{//场景数组
		"actived": false, //是否是活动场景，即是否当前选择场景
		"name": "Be right back",//场景名字
		"sources": [{//场景中的源，也是个数组
			"id": 2,
			"name": "Lovense Video Feedback 2",//源的名字
			"type": "lovense_video_freedback"//源的标识符，同一种源，其标识符一样
		}, {
			"id": 1,
			"name": "Media Source 3",
			"type": "ffmpeg_source"
		}]
	}, {
		"actived": false,
		"name": "Start soon (2)",
		"sources": [{
			"id": 2,
			"name": "Lovense Video Feedback 3",
			"type": "lovense_video_freedback"
		}, {
			"id": 1,
			"name": "Media Source 2",
			"type": "ffmpeg_source"
		}]
	}, {
		"actived": true,
		"name": "Start soon",
		"sources": [{
			"id": 3,
			"name": "Lovense Video Feedback 4",
			"type": "Lovese_browser_source"
		}, {
			"id": 2,
			"name": "Lovense Video Feedback",
			"type": "lovense_video_freedback"
		}, {
			"id": 1,
			"name": "Media Source",
			"type": "ffmpeg_source"
		}]
	}]
}

```

+ **是否激活OBS摄像头** 

需求场景：OBS启动时会自动请求摄像头权限；为了将摄像头请求权限调至用户直播时；
此时，可以在用户点击进入某个网站直播时发送一次协议，这时自动激活摄像头权限；  
注意：默认OBS启动时不会请求摄像头权限，至少需要发送一次该协议；
```js
let msg_data = {
    request_type:'ActiveOBSCamera',
    message_id: '1358',
    active:true //激活为true,取消激活为false
};
```

+ **获取OBS画面** 

需求场景：将OBS画面通过websocket返回；   
```js
let msg_data = {
    request_type:'TakeSourceScreenshot',
    message_id: '1358',
    embedPictureFormat:'png', //必须指定一种格式；图片格式可以是 "png","bmp" 
	width:600,	//画面宽,最好是指定宽度，不然图片数据很大(省略时，返回OBS画面真实宽度)
	height:480, //画面高，最好是指定高度，不然图片数据很大(省略时，返回OBS画面真实高度)
	sourceName:'Scene Name',//可以省略，表示当前场景画面；也可以指定场景名字，获取指定场景画面
}; 

返回内容如：
{
	"img": "data:image/png;base64,....", //图片数据
	"message_id": "123",
	"sourceName": "Starting soon", //当前场景的名字
	"status": "ok"
}
```

+ **插件更新完成通知** 

需求场景：插件热更新。收到该协议时，插件已经更新到最新版本。可以重新加载最新插件
```js
//由websocket server对外广播
let broadcast_data = {
    update_type:'ScriptUpdated'
};

example data:
{
	"update_type":"ScriptUpdated"
}
