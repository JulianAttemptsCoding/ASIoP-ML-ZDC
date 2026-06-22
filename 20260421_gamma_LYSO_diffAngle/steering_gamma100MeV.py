import sys
import math
import argparse

from DDSim.DD4hepSimulation import DD4hepSimulation
from g4units import cm, mm, GeV, MeV, degree, radian
SIM = DD4hepSimulation()

#energyMin = "1*GeV"
#energyMax = "20*GeV"
#particle = "gamma"
energyMin = "100*MeV"
energyMax = "100*MeV"
particle = "gamma" #e+, don't use positron 

ionCrossingAngle = -0.025 * radian
ZDC_r_pos = 3550 * cm
ZDC_x_pos = 0
ZDC_y_pos = 0 
ZDC_z_pos = ZDC_r_pos
shift = 1.5 * cm

SIM.numberOfEvents = 1000
SIM.enableGun = True
SIM.gun.particle = particle
SIM.gun.momentumMin = eval(energyMin)
SIM.gun.momentumMax = eval(energyMax)
SIM.gun.distribution = "uniform" 
SIM.gun.multiplicity = 1

SIM.gun.position = (ZDC_x_pos-shift, ZDC_y_pos-shift, ZDC_z_pos)
SIM.gun.thetaMin = 0* degree
SIM.gun.thetaMax = 25* degree
SIM.gun.phiMin = 0* degree
SIM.gun.phiMax = 360* degree

