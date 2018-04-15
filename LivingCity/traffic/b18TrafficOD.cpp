#include "b18TrafficOD.h"

#include "../roadGraphB2018Loader.h"

#define ROUTE_DEBUG 0
#define PERSON_DEBUG 0

namespace LC {

B18TrafficOD::B18TrafficOD() {
}//
B18TrafficOD::~B18TrafficOD() {
}//

void B18TrafficOD::sampleDistribution(int numberToSample,
                                      PeopleJobOneLayer &dist, std::vector<QVector2D> &samples, QString &name) {

  QMap<int, int> posInArrayIndexInSamples;
  int posInArray = 0;
  std::vector<float> accProbability;

  for (int mP = 0; mP < dist.amplitudes.size(); mP++) {
    float ampl = dist.amplitudes[mP];

    if (ampl > 0) {
      float accProb = ampl;

      if (accProbability.size() > 0) {
        accProb += accProbability[accProbability.size() - 1];
      }

      accProbability.push_back(accProb);
      posInArrayIndexInSamples.insert(posInArray, mP);
      posInArray++;
    }
  }

  int maxAccValue = accProbability[accProbability.size() - 1];

  samples.resize(numberToSample);
  std::random_device rd;
  std::normal_distribution<float> distSam(0, dist.stdev);
  std::mt19937 rand_engine(rd());

  for (int nSam = 0; nSam < numberToSample; nSam++) {
    // find amplitude
    float randAmpl = (((float)qrand()) / RAND_MAX) *
                     maxAccValue; //sample in the acc
    int aN;
    bool found = true;

    for (aN = 0; aN < accProbability.size(); aN++) {
      if (accProbability[aN] > randAmpl) {
        found = true;
        break;
      }
    }

    //printf("aN %d randAmpl %f accPro[aN] %f\n",aN,randAmpl,accProbability[aN]);
    if (found == false) {
      printf("ERROR randAmpl not found\n");
    }

    // sample gaussian nad create sample
    samples[nSam].setX(distSam(rand_engine) +
                       dist.samplePosition[posInArrayIndexInSamples[aN]].x());
    samples[nSam].setY(distSam(rand_engine) +
                       dist.samplePosition[posInArrayIndexInSamples[aN]].y());
  }

  if (name.length() > 0) {
    if (PERSON_DEBUG) {
      printf(">> >> >> 2.1. normalize image to check and save maxAccValue %d numSample %d\n",
             maxAccValue, numberToSample);
    }

    cv::Mat peopleSelecF = cv::Mat::zeros(1024, 1024, CV_32FC1);
    cv::Mat peopleSelecU = cv::Mat::zeros(1024, 1024, CV_8UC1);
    float maxDesity = 0;
    int r, c;

    for (int nSam = 0; nSam < numberToSample; nSam++) {
      r = ((samples[nSam].y() / G::global().getFloat("worldWidth")) + 0.5f) *
          peopleSelecF.rows;
      c = ((samples[nSam].x() / G::global().getFloat("worldWidth")) + 0.5f) *
          peopleSelecF.cols;

      if (r < 0) {
        r = 0;
      }

      if (r >= peopleSelecF.rows) {
        r = peopleSelecF.rows - 1;
      }

      if (c < 0) {
        c = 0;
      }

      if (c >= peopleSelecF.rows) {
        c = peopleSelecF.cols - 1;
      }

      peopleSelecF.at<float>(r, c) += 1.0f;

      if (maxDesity < peopleSelecF.at<float>(r, c)) {
        maxDesity = peopleSelecF.at<float>(r, c);
      }
    }

    if (PERSON_DEBUG) {
      printf("maxDensity %f\n", maxDesity);
    }

    for (int c = 0; c < peopleSelecU.cols; c++) {
      for (int r = 0; r < peopleSelecU.rows; r++) {
        peopleSelecU.at<uchar>(r, c) = (uchar)(255.0f * peopleSelecF.at<float>(r,
                                               c) / maxDesity);
      }
    }

    cv::imwrite(name.toLatin1().constData(), peopleSelecU);
  }

  if (PERSON_DEBUG) {
    printf("<<sampleDistribution\n");
  }
}

void B18TrafficOD::randomPerson(int p, CUDATrafficPerson &person,
                                uint srcvertex,
                                uint tgtvertex, float startTime) {

  // Data
  person.init_intersection = srcvertex;
  person.end_intersection = tgtvertex;
  person.time_departure = startTime * 3600.0f; //seconds
  //printf("Person %d: init %u end %u Time %f\n",p,srcvertex,tgtvertex,goToWork);
  // Status
  qsrand(p);
  person.a = 1.0f + ((float) qrand()) / RAND_MAX; //acceleration 1-2m/s2
  person.b = 1.0f + ((float) qrand()) / RAND_MAX; //break 1-2m/s2
  person.T = 0.8f + 1.2f * (((float) qrand()) / RAND_MAX); //time heading 0.8-2s
  person.v = 0;
  person.num_steps = 0;
  person.gas = 0;
  person.active = 0;
  person.numOfLaneInEdge = 0;
  person.color = p << 8;
  person.LC_stateofLaneChanging = 0;
  person.personPath[0] = -1;

}

void B18TrafficOD::randomPerson(int p, CUDATrafficPerson &person,
                                QVector3D housePos3D, QVector3D jobPos3D,
                                float startTime,
                                LC::RoadGraph::roadBGLGraph_BI &roadGraph) {
  LC::RoadGraph::roadGraphVertexDesc srcvertex, tgtvertex;
  RoadGraph::roadGraphVertexIter vi, viEnd;

  // 3.3 find closest nodes
  QVector2D hP = housePos3D.toVector2D();
  QVector2D jP = jobPos3D.toVector2D();
  float minHouseDis = FLT_MAX;
  float minJobDis = FLT_MAX;

  for (boost::tie(vi, viEnd) = boost::vertices(roadGraph);
       vi != viEnd; ++vi) {
    QVector2D roadVert(roadGraph[*vi].pt);
    float newHDist = (hP - roadVert).lengthSquared();

    if (newHDist < minHouseDis) {
      srcvertex = *vi;
      minHouseDis = newHDist;
    }

    float newJDist = (jP - roadVert).lengthSquared();

    if (newJDist < minJobDis) {
      tgtvertex = *vi;
      minJobDis = newJDist;
    }
  }

  randomPerson(p, person, srcvertex, tgtvertex, startTime);
}

void B18TrafficOD::resetTrafficPersonJob(
  std::vector<CUDATrafficPerson> &trafficPersonVec) {
  if (PERSON_DEBUG) {
    printf("trafficPersonVec %d backUpInitEdge %d\n", trafficPersonVec.size(),
           backUpInitEdge.size());
  }

  for (int p = 0; p < trafficPersonVec.size(); p++) {
  }
}//

void B18TrafficOD::createRandomPeople(
  int numberPerGen,
  float startTimeH, float endTimeH,
  std::vector<CUDATrafficPerson> &trafficPersonVec,
  PeopleJobInfoLayers &simPeopleJobInfoLayers,
  LC::RoadGraph::roadBGLGraph_BI &roadGraph) {
  if (PERSON_DEBUG) {
    printf(">> CUDA generateTrafficPersonJob\n");
  }

  QTime timer;
  //////////////////////////////////
  // 1. Generate PEOPLE
  timer.start();
  std::vector<QVector2D> peopleDis;
  QString people_dist_file("test/people_dist.png");
  sampleDistribution(numberPerGen, simPeopleJobInfoLayers.layers[1], peopleDis,
                     people_dist_file);

  if (PERSON_DEBUG) {
    printf(">> >> People %d sampled in %d ms\n", numberPerGen, timer.elapsed());
  }

  //////////////////////////////////
  // 2. Generate JOBS
  timer.restart();
  std::vector<QVector2D> jobDis;
  QString job_dist_file("test/job_dist.png");
  sampleDistribution(numberPerGen, simPeopleJobInfoLayers.layers[0], jobDis,
                     job_dist_file);

  if (PERSON_DEBUG) {
    printf(">> >> Job %d sampled in %d ms\n", numberPerGen, timer.elapsed());
  }

  // 3. Generate PEOPLE TRAFFIC
  timer.restart();
  trafficPersonVec.resize(numberPerGen);
  qsrand(6541799621);

  float midTime = (startTimeH + endTimeH) / 2.0f;

  for (int p = 0; p < numberPerGen; p++) {
    float goToWork = midTime + LC::misctools::genRand(startTimeH - midTime,
                     endTimeH - startTimeH);
    randomPerson(p, trafficPersonVec[p], peopleDis[p], jobDis[p], goToWork,
                 roadGraph);
  }

  if (PERSON_DEBUG) {
    printf(">> >> PeopleTraffic %d generated in %d ms\n", numberPerGen,
           timer.elapsed());
  }

  if (PERSON_DEBUG) {
    printf("<< CUDA generateTrafficPersonJob\n");
  }
}//

void B18TrafficOD::loadB18TrafficPeople(
  float startTimeH, float endTimeH,
  std::vector<CUDATrafficPerson> &trafficPersonVec, // out
  RoadGraph::roadBGLGraph_BI &roadGraph, int limitNumPeople) {

  trafficPersonVec.clear();
  QTime timer;
  timer.start();

  if (RoadGraphB2018::demandB2018.size() == 0) {
    printf("ERROR: Imposible to generate b2018 without loading b2018 demmand first\n");
    return;
  }

  int totalNumPeople = (limitNumPeople > 0) ? limitNumPeople : RoadGraphB2018::totalNumPeople;
  trafficPersonVec.resize(totalNumPeople);
  qsrand(6541799621);
  float midTime = (startTimeH + endTimeH) / 2.0f;

  int numPeople = 0;

  for (int d = 0; (d < RoadGraphB2018::demandB2018.size()) && (numPeople < totalNumPeople); d++) {
    int odNumPeople = std::min<int>(totalNumPeople - numPeople, RoadGraphB2018::demandB2018[d].num_people);
    uint src_vertex = RoadGraphB2018::demandB2018[d].src_vertex;
    uint tgt_vertex = RoadGraphB2018::demandB2018[d].tgt_vertex;

    for (int p = 0; p < odNumPeople; p++) {
      float goToWork = LC::misctools::genRand(startTimeH, endTimeH);/*midTime + LC::misctools::genRand(startTimeH - midTime,
                       endTimeH - startTimeH); //6.30-9.30 /// GOOOD ONE*/
     
      randomPerson(numPeople, trafficPersonVec[numPeople], src_vertex, tgt_vertex, goToWork);
     // printf("go to work %.2f --> %.2f\n", goToWork, (trafficPersonVec[p].time_departure / 3600.0f));

      numPeople++;
    }

  }

  if (totalNumPeople != numPeople) {
    printf("ERROR: generateB2018TrafficPeople totalNumPeople != numPeople, this should not happen.");
    exit(-1);
  }


  //print histogram
  float binLength = 0.166f;//10min
  float numBins = ceil((endTimeH - startTimeH) / binLength);
  printf("End time %.2f  Start time %.2f --> numBins %f\n", endTimeH, startTimeH, numBins);
  std::vector<int> bins(numBins);
  std::fill(bins.begin(), bins.end(), 0);

  for (int p = 0; p < trafficPersonVec.size(); p++) {
    // printf("depart %.2f\n", (trafficPersonVec[p].time_departure / 3600.0f));
    float t = (trafficPersonVec[p].time_departure / 3600.0f) - startTimeH;
    
    int binN = t / binLength;
    if (binN < 0 || binN >= numBins) {
      printf("ERROR: Bin out of range %d of %f\n", binN, numBins);
    }
    bins[binN]++;
  }

  printf("\n");

  for (int binN = 0; binN < bins.size(); binN++) {
    printf("%f %d\n", startTimeH + binN * binLength, bins[binN]);
  }

  printf("loadB18TrafficPeople: People %d\n", numPeople);

}

}

