#ifndef CONFIG_H_
#define CONFIG_H_

#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>

#include "PubSubClient.h"

#define VERSION "v0.9"

typedef struct
{
    String ssid;
    String pwd;
    String host;
} Config;
Config mConfig;

static const char HTTP_HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html lang='zh-CN'>
<head>
    <meta charset='UTF-8'>
    <meta name='viewport' content='width=device-width, initial-scale=1, user-scalable=no'/>
    <style>
        h4 {
            margin: 20px 0 0;
        }

        h5 {
            margin: 10px 0 0;
        }

        label {
            font-size: .6rem;
        }

        select {
            padding: 5px;
            font-size: 1em;
            margin-bottom: 5px;
            width: 99%;
        }

        input {
            padding: 5px;
            font-size: 1em;
            margin-bottom: 5px;
            width: 95%
        }

        body {
            text-align: center;
        }

        button {
            border: 0;
            border-radius: 1rem;
            background-color: #1fa3ec;
            color: #fff;
            line-height: 2rem;
            font-size: 1rem;
            width: 100%;
            margin: 0.5rem 0;
        }

        .file {
            position: relative;
            text-align: center;
            border-radius: 1rem;
            background-color: #1fa3ec;
            color: #fff;
            line-height: 2rem;
            margin: 0.5rem 0;
        }

        .file input {
            position: absolute;
            right: 0;
            top: 0;
            opacity: 0;
        }

        .container{
            text-align:left;
            display:inline-block;
            min-width:260px;
        }
    </style>
</head>
<body>
<div class='container'>
{container}
</div>
</body>
</html>
)";
const char HTTP_FORM_DATA[] PROGMEM = R"(
    <h4>固件版本: {version}</h4>
    <h5>主机名: {hostname}</h5>
    <h5 id='data'>{DATA}</h5>
    <form action='save' method='GET'>
        <h4>WiFi 配置</h4>
        <label>SSID:</label>
        <input name='ssid' value='{ssid}'>
        <br/>
        <label>密码:</label>
        <input name='pwd' value='{pwd}'>
        <br/>
        <h4>MQTT 配置</h4>
        <label>Host 地址:</label>
        <input name='host' value='{host}'>
        <br/>
        <button type='submit'>保存</button>
    </form>
    <form action='reset' method='GET' enctype='multipart/form-data'>
        <button type='submit'>重置</button>
    </form>
    <form action='upload' method='POST' enctype='multipart/form-data'>
        <div class="file">
            <input type='file' accept='.bin' name='firmware' onchange='submit()'>升级固件</input>
        </div>
    </form>

    <script>
        setInterval(
            function () { 
                var xhttp = new XMLHttpRequest(); 
                xhttp.onreadystatechange = function () {
                    if (this.readyState == 4 && this.status == 200) 
                        document.getElementById('data').innerHTML = this.responseText 
                };
                xhttp.open('GET', '/state', true); xhttp.send() }
            , 3000);
    </script>
)";
const char HTTP_SAVED[] PROGMEM = "<div><br/>正在保存配置并重启...</div>";
const char HTTP_CLEAN[] PROGMEM = "<div><br/>正在清除配置并重启...</div>";
const char HTTP_FIRMWARE[] PROGMEM = "<div><br/>升级{firmware}并重启...</div>";

#endif /* CONFIG_H_ */