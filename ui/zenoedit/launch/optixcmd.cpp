#include "AudioFile.h"
#include "zeno/extra/assetDir.h"
#define MINIMP3_IMPLEMENTATION
#define MINIMP3_FLOAT_OUTPUT
#include <QApplication>
#include <QTcpServer>
#include <QtWidgets>
#include <QTcpSocket>
#include <zeno/utils/log.h>
#include "zenomainwindow.h"
#include "settings/zsettings.h"
#include "zeno/core/Session.h"
#include "zeno/types/UserData.h"
#include "viewport/optixviewport.h"
#include "util/apphelper.h"
#include "common.h"
#include <zeno/core/Session.h>
#include <zeno/extra/GlobalComm.h>
#include <zeno/extra/GlobalState.h>

//#define DEBUG_DIRECTLY

int optixcmd(const QCoreApplication& app, int port)
{
    ZENO_RECORD_RUN_INITPARAM param;
#ifndef DEBUG_DIRECTLY

    //clone from recordmain.
    QCommandLineParser cmdParser;
    cmdParser.addHelpOption();
    cmdParser.addOptions({
        //name, description, value, default value.
        {"zsg", "zsg", "zsg file path"},
        {"optixcmd", "port", ""},
        {"frame", "frame", "frame count"},
        {"sframe", "sframe", "start frame"},
        {"sample", "sample", "sample count"},
        {"pixel", "pixel", "set record image pixel"},
        {"path", "path", "record dir"},
        {"audio", "audio", "audio path"},
        {"bitrate", "bitrate", "bitrate"},
        {"fps", "fps", "fps"},
        {"configFilePath", "configFilePath", "configFilePath"},
        {"cachePath", "cachePath", "cachePath"},
        {"cacheNum", "cacheNum", "cacheNum"},
        {"exitWhenRecordFinish", "exitWhenRecordFinish", "exitWhenRecordFinish"},
        {"optix", "optix", "optix mode"},
        {"video", "video", "export video"},
        {"aov", "aov", "aov"},
        {"needDenoise", "needDenoise", "needDenoise"},
        {"videoname", "videoname", "export video's name"},
        {"subzsg", "subgraphzsg", "subgraph zsg file path"},
        });
    cmdParser.process(app);

    if (cmdParser.isSet("zsg"))
        param.sZsgPath = cmdParser.value("zsg");
    param.bRecord = true;
    if (cmdParser.isSet("frame"))
        param.iFrame = cmdParser.value("frame").toInt();
    if (cmdParser.isSet("sframe"))
        param.iSFrame = cmdParser.value("sframe").toInt();
    if (cmdParser.isSet("sample"))
        param.iSample = cmdParser.value("sample").toInt();
    if (cmdParser.isSet("pixel"))
        param.sPixel = cmdParser.value("pixel");
    if (cmdParser.isSet("path"))
        param.sPath = cmdParser.value("path");
    if (cmdParser.isSet("configFilePath")) {
        param.configFilePath = cmdParser.value("configFilePath");
        zeno::setConfigVariable("configFilePath", param.configFilePath.toStdString());
    }
    QString cachePath;
    if (cmdParser.isSet("cachePath")) {
        cachePath = cmdParser.value("cachePath");
        cachePath.replace('\\', '/');
        if (!QDir(cachePath).exists()) {
            QDir().mkdir(cachePath);
        }
    }
    if (cmdParser.isSet("cacheNum")) {
    }
    if (cmdParser.isSet("exitWhenRecordFinish"))
        param.exitWhenRecordFinish = cmdParser.value("exitWhenRecordFinish").toLower() == "true";
    if (cmdParser.isSet("audio")) {
        param.audioPath = cmdParser.value("audio");
        //todo: resolve include compile problem(zeno\tpls\include\minimp3.h).
        //if (!cmdParser.isSet("frame")) {
        //    int count = calcFrameCountByAudio(param.audioPath.toStdString(), 24);
        //    param.iFrame = count;
        //}
    }
    param.iBitrate = cmdParser.isSet("bitrate") ? cmdParser.value("bitrate").toInt() : 20000;
    param.iFps = cmdParser.isSet("fps") ? cmdParser.value("fps").toInt() : 24;
    param.bOptix = cmdParser.isSet("optix") ? cmdParser.value("optix").toInt() : 0;
    param.isExportVideo = cmdParser.isSet("video") ? cmdParser.value("video").toInt() : 0;
    param.needDenoise = cmdParser.isSet("needDenoise") ? cmdParser.value("needDenoise").toInt() : 0;
    int enableAOV = cmdParser.isSet("aov") ? cmdParser.value("aov").toInt() : 0;
    auto& ud = zeno::getSession().userData();
    ud.set2("output_aov", enableAOV != 0);
    param.videoName = cmdParser.isSet("videoname") ? cmdParser.value("videoname") : "output.mp4";
    param.subZsg = cmdParser.isSet("subzsg") ? cmdParser.value("subzsg") : "";
#else
    param.sZsgPath = "C:\\zeno\\framenum.zsg";
    param.sPath = "C:\\recordpath";
    param.iFps = 24;
    param.iBitrate = 200000;
    param.iSFrame = 0;
    param.iFrame = 10;
    param.iSample = 1;
    param.bOptix = true;
    param.sPixel = "1200x800";
#endif

    VideoRecInfo recInfo = AppHelper::getRecordInfo(param);

    /*
    QTcpSocket optixClientSocket;
    optixClientSocket.connectToHost(QHostAddress::LocalHost, port);
    if (!optixClientSocket.waitForConnected(10000))
    {
        zeno::log_error("tcp optix client connection fail");
        return -1;
    }
    int iSize = optixClientSocket.write(std::string("optixCmd").data());
    if (!optixClientSocket.waitForBytesWritten(50000))
    {
        zeno::log_error("tcp init optix client connection fail");
        return -1;
    }
    if (iSize <= 0)
    {
        zeno::log_error("tcp init optix client send fail");
        return -1;
    }
    while (1) {
        int j;
        j = 0;
    }
    QObject::connect(&optixClientSocket, &QTcpSocket::readyRead, [&]() {
        QByteArray arr = optixClientSocket.readAll();
        qint64 redSize = arr.size();
        if (redSize > 0) {

            zeno::log_info("finish frame");
        }
    });
    */

    int beginF = recInfo.frameRange.first, endF = recInfo.frameRange.second;
    auto& globalComm = zeno::getSession().globalComm;

    globalComm->frameCache(cachePath.toStdString(), 1);
    globalComm->initFrameRange(beginF, endF);

    OptixWorker worker;
    for (int frame = beginF; frame <= endF; frame++)
    {
        globalComm->newFrame();
        //todo: check whether the cache file is ready.
        globalComm->finishFrame();
        bool ret = worker.recordFrame_impl(recInfo, frame);
    }
    return 0;
}
