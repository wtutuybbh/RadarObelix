#include "ObelixUdpSimThread.h"

ObelixUdpSimThread::ObelixUdpSimThread()
{
  mRange      = 100;
  mAntennaPos = 0;
  mAntennaRpm = 10;
  mBeamWidth  = 1;
  mTimerPeriod = 50;
  mVideoAzGapCorrectionRto = 1;
  mVideoAzGapLvlRto        = 2;
  mVideoRgGapLvlRto        = 100;
  mVideoIntensity          = 1;
  mVideoNoise              = 0;
  mVideoMode = OBX_VIDEO_SEARCH;
  mAntennaInverseRotate    = false;
  mAntennaInverseCurrentRotate    = false;
  mSectorScan              = false;
  mSectorPlatfromRef       = false;
  mSectorScanAzimuthDeg     = 0;
  mSectorScanWidthDeg      = 40;

  //
  mObelixUdpGene = nullptr;
}

void ObelixUdpSimThread::SetUdpReaderParameters(QString pVideoIp, uint pVideoPort,
                                                QString pTrackIp, uint pTrackPort,
                                                QString pMapIp  , uint pMapPort)
{
  mVideoIp = pVideoIp;
  mVideoPort = pVideoPort;
  mTrackIp = pTrackIp;
  mTrackPort = pTrackPort;
  mMapIp     = pMapIp;
  mMapPort  = pMapPort;
}

void ObelixUdpSimThread::SetVideoBeamParameters(int pNbLevel, int pNbCells)
{
  mBeamNbLevel = pNbLevel;
  mBeamNbCells = pNbCells;
}

void ObelixUdpSimThread::SetVideoIntensity(double pVal)
{
  if (mObelixUdpGene)
  {
    mObelixUdpGene->SetVideoIntensity(pVal);
  }
  mVideoIntensity = pVal;
}

double ObelixUdpSimThread::VideoIntensity()
{
  if (mObelixUdpGene)
  {
    mVideoIntensity = mObelixUdpGene->VideoIntensity();
  }

  return mVideoIntensity;
}

void ObelixUdpSimThread::SetVideoNoise(double pVal)
{
  if (mObelixUdpGene)
  {
    mObelixUdpGene->SetVideoNoise(pVal);
  }
  mVideoNoise = pVal;
}

double ObelixUdpSimThread::VideoNoise()
{
  if (mObelixUdpGene)
  {
    mVideoNoise = mObelixUdpGene->VideoNoise();
  }

  return mVideoNoise;
}

void ObelixUdpSimThread::PushTracks(T_ObelixTrack *pTrackTable, uint pCount)
{
  // Loop on tracks
  for (uint i=0; i<pCount; i++)
  {
    // Update
    if (mTrackTable.contains(pTrackTable[i].Id) == true)
    {
      mTrackTable[pTrackTable[i].Id] = pTrackTable[i];
    }
    // Add
    else
    {
      mTrackTable.insert(pTrackTable[i].Id, pTrackTable[i]);
    }
  }

  //qDebug("TableCnt=%i", mTrackTable.count());
/*
  QHash<uint16_t, T_ObelixTrack>::iterator lTrackIter;
  int i=0;
  for (lTrackIter = mTrackTable.begin(); lTrackIter != mTrackTable.end(); ++lTrackIter)
  {
    qDebug("[%i]Id:%i", i,lTrackIter.value().Id);
    i++;
  }*/
}

bool ObelixUdpSimThread::PushMapObject(uint16_t          pId,
                                       uint8_t           pType,
                                       T_ObelixMapPoint* pPointTable,
                                       uint              pCount)
{
  if (nullptr == mObelixUdpGene)
  {
    return false;
  }

  return mObelixUdpGene->PushMapObject(pId, pType, pPointTable, pCount);
}

void ObelixUdpSimThread::SetPlatformPosition(double pLatitude, double pLongitude)
{
  if (nullptr == mObelixUdpGene)
  {
    return;
  }

  return mObelixUdpGene->SetPlatformPosition(pLatitude, pLongitude);
}

void ObelixUdpSimThread::AskForStop()
{
  mRun = false;
  exit();
  delete mObelixUdpGene;
  mObelixUdpGene = nullptr;
}


void ObelixUdpSimThread::run()
{


  qDebug("Run is executed in thread : %X", currentThreadId());




  mObelixUdpGene = new ObelixUdpSim();


  mObelixUdpGene->SetUdpWriterVideoParameters(mVideoIp, mVideoPort);
  mObelixUdpGene->SetUdpWriterTrackParameters(mTrackIp, mTrackPort);
    mObelixUdpGene->SetUdpWriterMapParameters(mMapIp, mMapPort);
  mObelixUdpGene->SetVideoBeamParameters(mBeamNbLevel, mBeamNbCells);
  mObelixUdpGene->SetTrackTableRef(&mTrackTable);



  mObelixUdpGene->SetVideoIntensity(mVideoIntensity);
   mObelixUdpGene->SetVideoNoise(mVideoNoise);


  mObelixUdpGene->moveToThread(this);

  mTimer = new QTimer();
  mTimer->moveToThread(this);
  connect(mTimer , &QTimer::timeout, this, &ObelixUdpSimThread::OnTimerTick, Qt::DirectConnection);
  mTimer->start(mTimerPeriod);
  exec();

}

void ObelixUdpSimThread::OnTimerTick()
{
  // Video beams

  //
  double lTickBeamWidth  = static_cast<double>(mTimerPeriod) * 6 * mAntennaRpm/1000.0;
  uint lTickBeamRepeat =  static_cast<uint>(ceil(lTickBeamWidth/mBeamWidth));

  // Beam repetition
  for (uint lIdxBeam=0; lIdxBeam<lTickBeamRepeat; lIdxBeam++)
  {
    // Send video beam
    mObelixUdpGene->SendVideoBeam(mPlatformHeading, mAntennaPos,mBeamWidth,0,mRange,mVideoMode);

    double lLastAntennaPos = mAntennaPos;

    // Update antenna position according rotation way
    if (mAntennaInverseCurrentRotate == false)
    {
      mAntennaPos += mBeamWidth;
    }
    else
    {
      mAntennaPos -= mBeamWidth;
    }

    // Lap correction
    auto lapCorrection = [](double lAngle) -> double {return lAngle >= 360 ? (lAngle-360) : (lAngle <0 ? (lAngle+360) : (lAngle)); };

    // Lap correction
    mAntennaPos = lapCorrection(mAntennaPos);

    // Sector scan?
    if (mSectorScan == true)
    {
      double lSectorScanAzimutOffet = mSectorScanAzimuthDeg + (mSectorPlatfromRef ? mPlatformHeading : 0);
      double lForwardStop = lapCorrection(lSectorScanAzimutOffet + 0.5*mSectorScanWidthDeg);
      double lRewindStop  = lapCorrection(lSectorScanAzimutOffet - 0.5*mSectorScanWidthDeg);

      // Forward way
      if ((mAntennaInverseCurrentRotate == false       ) &&
          (lLastAntennaPos              <= lForwardStop) &&
          (mAntennaPos                  >= lForwardStop))
      {
        mAntennaInverseCurrentRotate = true;
      }

      // Reverse way
      if((mAntennaInverseCurrentRotate == true       ) &&
         (lLastAntennaPos              >= lRewindStop) &&
         (mAntennaPos                  <= lRewindStop))
      {
        mAntennaInverseCurrentRotate = false;
      }
    }
  }

  /// \todo Do less
  // Send Track report
  mObelixUdpGene->SendTrackTable();

  // Send Map report
  mObelixUdpGene->SendObjectMapTable();
}
