from PyQt4 import QtGui, QtCore, uic
import pyqtgraph as pg
import math
import numpy as np
import time
import serial
import sys
import csv
from scipy.signal import medfilt
import operator
from sklearn import svm
import sklearn


sys.dont_write_bytecode = True

from pprint import pprint, pformat

import pyble
from pyble.handlers import PeripheralHandler, ProfileHandler
from pyble._roles import Central, Peripheral

import time
import struct

import binascii

port  = "/dev/tty.CaRE_Bluetooth-SPPDev"
baudrate = 115200
                                              
qtMainWindowFile = "care_ui.ui"
qtDialogFile = "care_dialog.ui"
Ui_MainWindow, QtBaseClass = uic.loadUiType(qtMainWindowFile)
Ui_Dialog, QtBaseClass = uic.loadUiType(qtDialogFile)

duration = 5                                              
rest_hr = 60                                              
min_target_hr = 105                                       
max_target_hr = 110
rpe = 1


class MyDefault(ProfileHandler):
    UUID = "3D9FFEC0-50BB-3960-8782-C593EDBC35EA"
    _AUTOLOAD = False
    names = {
        "3D9FFEC0-50BB-3960-8782-C593EDBC35EA": "EcoZen Profile",
        "3D9FFEC1-50BB-3960-8782-C593EDBC35EA": "EcoZen Char 1",
        "3D9FFEC2-50BB-3960-8782-C593EDBC35EA": "EcoZen Char 2",
        "3D9FFEC3-50BB-3960-8782-C593EDBC35EA": "EcoZen Char 3"
    }

    def initialize(self):
        print "init"
        pass

    def on_read(self, characteristic, data):
        ans = []
        for b in data:
            ans.append("%02X" % ord(b))
            ret = "0x" + "".join(ans)
            return ret

class MyPeripheral(PeripheralHandler):
    
    def initialize(self):
        self.addProfileHandler(MyDefault)
    
    def on_connect(self):
        print self.peripheral, "connect"
    
    def on_disconnect(self):
        print self.peripheral, "disconnect"
    
    def on_rssi(self, value):
        print self.peripheral, " update RSSI:", value

    
class Care_App_InitDiag(QtGui.QDialog, Ui_Dialog):
    def __init__(self, parent = None):
        QtGui.QDialog.__init__(self)
        Ui_Dialog.__init__(self)
        self.setupUi(self)
        
        self.setWindowTitle("CaRE")
        self.setWindowIcon(QtGui.QIcon('design/care_logo.png'))
        p = self.palette()
        p.setColor(self.backgroundRole(),QtCore.Qt.white)        
        self.setPalette(p)
        
        
        regex = QtCore.QRegExp("[0-9_]+")
        validator = QtGui.QRegExpValidator(regex)
        
        self.duration = duration
        self.duration_lineEdit.setMaxLength(2)
        self.duration_lineEdit.setValidator(validator)
        self.duration_lineEdit.setText("{0}".format(duration))
        
        self.rest_hr = rest_hr
        self.restinghr_lineEdit_2.setMaxLength(3)
        self.restinghr_lineEdit_2.setValidator(validator)
        self.restinghr_lineEdit_2.setText("{0}".format(rest_hr))

        
        self.max_target_hr = max_target_hr
        self.maxtargethr_lineEdit_4.setMaxLength(3)
        self.maxtargethr_lineEdit_4.setValidator(validator)
        self.maxtargethr_lineEdit_4.setText("{0}".format(max_target_hr))
        
        self.rpe= rpe
        self.borg_rpe.setMaxLength(1)
        self.borg_rpe.setValidator(validator)
        self.borg_rpe.setText("{0}".format(rpe))
        
        self.confirm_pushButton.clicked.connect(self.onclick_Confirm)

        
        
    def onclick_Confirm(self):        
        try: 
            self.duration = int(self.duration_lineEdit.text())
            self.rest_hr = int(self.restinghr_lineEdit_2.text())
            self.max_target_hr = int(self.maxtargethr_lineEdit_4.text())
            self.rpe = int(self.borg_rpe.text())
            
            if ((self.duration != 0) & (self.max_target_hr != 0)): 
                self.window = Care_App(self.duration,self.rest_hr,self.max_target_hr, self.rpe)
                self.window.show()
                self.deleteLater()
                
            else: 
                self.errorhandle_label_5.setText("A value of zero is invalid")
        except ValueError:
            self.errorhandle_label_5.setText("Please fill all fields")

        


class Care_App(QtGui.QMainWindow, Ui_MainWindow):

    def __init__(self,duration,resthr,maxhr,rpe,parent=None):
        QtGui.QMainWindow.__init__(self)
        Ui_MainWindow.__init__(self)
        self.setupUi(self)         
        
        self.setWindowTitle("CaRE: Cardiovascular Rehabilitation Equipment")
        self.setWindowIcon(QtGui.QIcon('design/care_logo.png'))
        
        self.connectstatus_label_28.setText("The CaRE Bluetooth is currently disconnected.")
        self.connectstatus_label_28.setStyleSheet('color: red')
        
        self.movesense_connection.setText("Movesense is currently disconnected.")
        self.movesense_connection.setStyleSheet('color: red')
        
        self.cadence_status_label_8.setText(" ")
        
        self.spo2_status_label_9.setText(" ")
        
        p = self.palette()
        p.setColor(self.backgroundRole(),QtCore.Qt.white)        
        self.setPalette(p)
        
        self.reset_pushButton.clicked.connect(self.reset_System)
        
        self.duration = duration
        self.rest_hr = resthr
        self.max_target_hr = maxhr
        self.rpe = rpe
        
        if (self.duration <= 1):
            duration_string = "%d minute" % (self.duration)
        else:
            duration_string = "%d minutes" % (self.duration)
                
        self.duration_label_29.setText(duration_string)
        self.maxtargethr.setText("{0}".format(int(self.max_target_hr)))
        self.session_label_31.setText(" ")
        
        self.ecg = np.linspace(0,0,16)
        
        self.motor_hr1 = 0
        self.motor_hr2 = 0
        self.previoushexhr = 0
        self.timestamp_buffer = 0
        self.arr_ecg = []
        self.arr_hr = []
        self.motor_ctr = 0
        self.target_hr = 0
        self.int_resistance = 0
        
        
        self.target_slope = float(self.max_target_hr - self.rest_hr)
        self.target_slope = float(self.target_slope/(((duration*60)/3)))
        print "Target Slope:"
        print self.target_slope
        
        self.target_slope_cooldown = float(self.max_target_hr - self.rest_hr)
        self.target_slope = float(self.target_slope/(((duration*60*3)/4)))
        print "Target Slope - Cool Down:"
        print self.target_slope_cooldown
        
        
        #Push Button Event Handling for Connect Button
        self.connect_button.clicked.connect(self.connectbutton_clicked)
        
        #Push Button Event Handling
        self.start_exercise.clicked.connect(self.startexercise_clicked)

        
    def connectbutton_clicked(self):
        self.connectstatus_label_28.setText("Connecting to CaRE System Box . . .")
        self.connectstatus_label_28.setStyleSheet('color: orange')
        self.connect_arduino()
        self.connectstatus_label_28.setText("The CaRE Bluetooth is currently connected.")
        self.connectstatus_label_28.setStyleSheet('color: green')
        self.movesense_connection.setText("Connecting to Movesense . . .")
        self.movesense_connection.setStyleSheet('color: orange')
        self.connect_movesense()
        self.movesense_connection.setText("Movesense is connected")
        self.movesense_connection.setStyleSheet('color: green')
            
            
    def startexercise_clicked(self):
        self.sendinputparameters()
        self.sendtoarduino(1) #Send Start Bit
        self.starttime = time.time()
        
        self.hr_widget = self.CustomWidgetHR(self.duration,self.rest_hr,self.max_target_hr)
        self.hr_widget.setStyleSheet("margin:4px; border:2px solid rgb(37,92,153)")
        self.verticalLayout_12.addWidget(self.hr_widget)
        
        self.ecg_widget = self.CustomWidgetECG(self.duration)
        self.ecg_widget.setStyleSheet("margin:4px; border:2px solid rgb(37,92,153)")
        self.verticalLayout_15.addWidget(self.ecg_widget)
        
        self.subscribe_movesense()
        self.update_time()
        #Initialize Timer 1
        self.timer1 = pg.QtCore.QTimer(self)
        self.timer1.timeout.connect(lambda: self.update1())
        self.timer1.start(0) #Call update_time function continuously
    
        #Initialize Timer 2
        self.timer2 = pg.QtCore.QTimer(self)
        self.timer2.timeout.connect(lambda: self.update2())
        self.timer2.start(950) #Call update_time function continuously
    
    def connect_arduino(self):
        self.cm1 = pyble.CentralManager()
        if not self.cm1.ready:
            return
        self.target1 = None
        while True:
            try:
                self.target1 = self.cm1.startScan()
                if self.target1 and self.target1.name == "CaRE":
                    print self.target1
                    break
            except Exception as e:
                print e
        self.target1.delegate = MyPeripheral


    def connect_movesense(self):
        self.cm = pyble.CentralManager()
        if not self.cm.ready:
            return
        self.target = None
        while True:
            try:
                self.target = self.cm.startScan()
                if self.target and self.target.name == "Movesense 184130000973":
                    print self.target
                    break
            except Exception as e:
                print e
        self.target.delegate = MyPeripheral

    def sendinputparameters(self):
        p = self.cm1.connectPeripheral(self.target1)
        c1 = p["293f365c-d247-4426-9ceb-a466378d457e"]["293f365c-d247-4426-9ceb-a466378d457e"] #Arduino UUID
        data = [self.rpe, self.duration, self.rest_hr, self.max_target_hr]
        byteexertime = bytearray(data)
        pprint(byteexertime)
        c1.value = byteexertime
        print "Value sent:"
        print c1.value

    def sendtoarduino(self, data):
        p = self.cm1.connectPeripheral(self.target1)
        c1 = p["293f365c-d247-4426-9ceb-a466378d457e"]["293f365c-d247-4426-9ceb-a466378d457e"] #Arduino UUID
        sdata = [data]
        byteexertime = bytearray(sdata)
        pprint(byteexertime)
        c1.value = byteexertime
        print "Value sent:"
        print c1.value
    
    def subscribe_movesense(self):
        p = self.cm.connectPeripheral(self.target)
        self.c = p["1809"]["2A1C"] #Custom GATT UUID
        self.c.notify = True
                
    def update1(self):
        if (self.currenttime) <= (self.duration*60):
            self.movesense_parsedata()
            self.update_hr()
            self.update_time()
            self.update_sessionstatus()
            self.update_ecg()
            self.arduino_parsedata()
        else:
            resultFile = open("/Users/jemuelbryanvergara/Documents/hr.csv",'wb')
            wr = csv.writer(resultFile, delimiter = ";")
            wr.writerows([self.arr_hr])
            self.stop_system()

    def update2(self):
        if (self.currenttime) <= (self.duration*60):
            self.update_time()
            self.update_sessionstatus()
            self.update_ecg()
            self.arduino_parsedata()
        else:
            resultFile = open("/Users/jemuelbryanvergara/Documents/hr.csv",'wb')
            wr = csv.writer(resultFile, delimiter = ";")
            wr.writerows([self.arr_hr])
            self.stop_system()

    def movesense_parsedata(self):
        if self.c._value == None:
            pass
        else:
            hexdata = binascii.hexlify(self.c._value)
            hextype = hexdata[0:2]
            print hextype
            type = int(hextype, 16)
            if type == 1:
                print("ECG Notification")
                hextimestamp = hexdata[2:10]
                self.timestamp = int(hextimestamp, 16)
                i = 10
                j = 0
                while i<71:
                    hexecg = hexdata[i:i+4]
                    intecg = int(hexecg, 16)
                    if intecg >= 32768:
                        intecg -= 65536
                    self.ecg[j] = intecg
                    i = i+4
                    j = j+1
                print "Array length:"
                print len(self.arr_ecg)
                if (self.seconds % 10) != 0:
                    self.arr_ecg.append(self.ecg)
                    print("Array appended")
                else:
                    ####CALL ARRHYTHMA ANALYSIS####
                    d = 0
                    while d < len(self.arr_ecg):
                        self.arr_ecg[d] = 0
                        d = d+1
            if type == 2:
                print("HR Notification")
                hexhr = hexdata[2:6]
                #hexrrinterval = hexdata[6:10]
                self.hr = int(hexhr, 16)
                self.arr_hr.append(self.hr)
                #self.rrinterval = int(hexrrinterval, 16)
                print "HR: "
                print self.hr
                #Change Bike Resistance eveery 5 sec based on HR
                if ((self.seconds % 5) == 0):
                    if (self.currenttime <= ((self.duration*60)/3)):
                        self.target_hr = float(((self.target_slope*self.currenttime) + self.rest_hr))
                        if (self.target_hr > self.hr):
                            self.motor_control((self.int_resistance + 10))
                        elif (self.target_hr < self.hr):
                            self.motor_control((self.int_resistance - 10))
                    elif ((self.currenttime > float(self.duration * 60)/3) and (self.currenttime< float(self.duration * 60)*0.75)):
                        if (self.max_target_hr > self.hr):
                            self.motor_control((self.int_resistance + 10))
                        elif (self.max_target_hr < self.hr):
                            self.motor_control((self.int_resistance - 10))
                    else:
                        self.target_hr = float((self.rest_hr) - (self.target_slope*self.currenttime))
                        if (self.target_hr > self.hr):
                            self.motor_control((self.int_resistance + 10))
                        elif (self.target_hr < self.hr):
                            self.motor_control((self.int_resistance - 10))

    def update_hr(self):
        self.hr_widget.plot(self.hr)
        self.currenthr.setText("{0}".format(self.hr))
    
    def update_time(self):
        self.currenttime = int(time.time() - self.starttime)
        if self.currenttime <= (self.duration*60):
            if self.currenttime >= 60:
                self.seconds = self.currenttime % 60
                self.minutes = self.currenttime/60
            else:
                self.seconds = self.currenttime
                self.minutes = 0
            self.time_elapsed = "{:02d}:{:02d}".format(self.minutes,self.seconds)
            self.timer_label_30.setText(self.time_elapsed)
        else:
            self.stop_system()

    def update_sessionstatus(self):
        if (self.currenttime < float(self.duration * 60)/3):
            self.session_label_31.setText("Warm Up Phase")
        elif (self.currenttime > float(self.duration * 60)/3 and self.currenttime< float(self.duration * 60)*0.75):
            self.session_label_31.setText("Main Phase")
        else:
            self.session_label_31.setText("Cool Down Phase")

    def update_ecg(self):
        if self.timestamp != self.timestamp_buffer:
            self.timestamp_buffer = self.timestamp
            print self.timestamp
            self.ecg_widget.plot(self.ecg)

    def arduino_parsedata(self):
        self.readarduino()
        received_value = self.c1.value
        twobytesindicator = received_value[10:14]
        print twobytesindicator
        byteindicator = received_value[5:9]
        print byteindicator
        if byteindicator =="0x43":
            print "SpO2 Data"
            hexspo2 = received_value[0:4]
            spo2 = int(hexspo2, 16)
            print "SpO2 Level:"
            print spo2
            self.spo2.setText("{0}".format(spo2))
            if spo2 >= 90 and spo2 <=100:
                self.spo2_status_label_9.setText("Safe level of SpO2 detected")
                self.spo2_status_label_9.setStyleSheet('color: green')
            else:
                self.spo2_status_label_9.setText("Dangerous level of SpO2 detected")
                self.spo2_status_label_9.setStyleSheet('color: red')
        elif twobytesindicator == "0x48":
            print "BP Data"
            hexsystole = received_value[5:9]
            systole = int(hexsystole, 16)
            hexdiastole = received_value[0:4]
            diastole= int(hexdiastole, 16)
            print "Measured BP:"
            print systole
            print "/"
            print diastole
            self.bpsys.setText("{0}".format(systole))
            self.bpdias.setText("{0}".format(diastole))
            if (systole == 33 or diastole == 33):
                self.bpsys.setText("0")
                self.bpdias.setText("0")
                self.bp_status.setText("Check BP Cuff Placement")
                self.bp_status.setStyleSheet('color: red')
            elif (systole <= 90 or systole >= 200 or diastole <= 60 or diastole >= 110):
                self.bp_status.setText("Dangerous BP Level Detected")
                self.bp_status.setStyleSheet('color: red')
            else:
                self.bp_status.setText("Safe BP Level Detected")
                self.bp_status.setStyleSheet('color: green')
        elif byteindicator == "0x45":
            print "Cadence Data"
            hex_rpm = received_value[0:4]
            int_rpm = int(hex_rpm, 16)
            print "RPM:"
            print int_rpm
            self.cadence.setText("{0}".format(int_rpm))
            if int_rpm > 95:
                self.cadence_status_label_8.setText("Too Fast")
                self.cadence_status_label_8.setStyleSheet('color: red')
            elif int_rpm < 85:
                self.cadence_status_label_8.setText("Too Slow")
                self.cadence_status_label_8.setStyleSheet('color: red')
            elif (int_rpm > 85) and (int_rpm < 95):
                self.cadence_status_label_8.setText("On Target")
                self.cadence_status_label_8.setStyleSheet('color: green')
        elif twobytesindicator == "0x46":
            print "Resistance Level Data"
            hex_resistance = received_value[5:9] + received_value[2:4]
            self.int_resistance = int(hex_resistance, 16)
            print "Resistance Level:"
            print self.int_resistance
            self.int_resistance = float(self.int_resistance-500)
            self.int_resistance =  float(self.int_resistance/520)
            self.int_resistance = math.floor(self.int_resistance*100)
            print self.int_resistance
            self.resistance.setText("{0}".format(self.int_resistance))
        elif byteindicator == "0x47":
            print "Borg Data"
            hexborg = received_value[0:4]
            borgvalue= int(hexborg, 16)
            print "Borg's Scale Button no:"
            print borgvalue
            if borgvalue == 1:
                self.borg_status_label_9.setText("REST")
                self.borg_status_label_9.setStyleSheet('color: blue')
            elif borgvalue == 2:
                self.borg_status_label_9.setText("REALLY EASY")
                self.borg_status_label_9.setStyleSheet('color: blue')
                if borgvalue > self.rpe:
                    self.borg_status_label_9.setText("REALLY EASY - RPE BEYOND TARGET")
                    self.borg_status_label_9.setStyleSheet('color: red')
            elif borgvalue == 3:
                self.borg_status_label_9.setText("Borg's Scale: EASY")
                self.borg_status_label_9.setStyleSheet('color: blue')
                if borgvalue > self.rpe:
                    self.borg_status_label_9.setText("EASY - RPE BEYOND TARGET")
                    self.borg_status_label_9.setStyleSheet('color: red')
            elif borgvalue == 4:
                self.borg_status_label_9.setText("Borg's Scale: MODERATE")
                self.borg_status_label_9.setStyleSheet('color: green')
                if borgvalue > self.rpe:
                    self.borg_status_label_9.setText("MODERATE - RPE BEYOND TARGET")
                    self.borg_status_label_9.setStyleSheet('color: red')
            elif borgvalue == 5:
                self.borg_status_label_9.setText("Borg's Scale: SORT OF HARD")
                self.borg_status_label_9.setStyleSheet('color: green')
                if borgvalue > self.rpe:
                    self.borg_status_label_9.setText("SORT OF HARD - RPE BEYOND TARGET")
                    self.borg_status_label_9.setStyleSheet('color: red')
            elif borgvalue == 6:
                self.borg_status_label_9.setText("Borg's Scale: HARD")
                self.borg_status_label_9.setStyleSheet('color: green')
                if borgvalue > self.rpe:
                    self.borg_status_label_9.setText("HARD - RPE BEYOND TARGET")
                    self.borg_status_label_9.setStyleSheet('color: red')
            elif borgvalue == 7:
                self.borg_status_label_9.setText("Borg's Scale: REALLY HARD")
                self.borg_status_label_9.setStyleSheet('color: red')
                if borgvalue > self.rpe:
                    self.borg_status_label_9.setText("REALLY HARD - RPE BEYOND TARGET")
                    self.borg_status_label_9.setStyleSheet('color: red')
            elif borgvalue == 8:
                self.borg_status_label_9.setText("Borg's Scale: REALLY, REALLY HARD")
                self.borg_status_label_9.setStyleSheet('color: red')
                if borgvalue > self.rpe:
                    self.borg_status_label_9.setText("REALLY, REALLY HARD - RPE BEYOND TARGET")
                    self.borg_status_label_9.setStyleSheet('color: red')
            elif borgvalue == 9:
                self.borg_status_label_9.setText("Borg's Scale: MAXIMAL EXERTION")
                self.borg_status_label_9.setStyleSheet('color: red')
                if borgvalue > self.rpe:
                    self.borg_status_label_9.setText("MAXIMAL EXERTION - RPE BEYOND TARGET")
                    self.borg_status_label_9.setStyleSheet('color: red')
            #self.bpsys_lcdNumber.display(systole)
    

    def motor_control(self, resistance_value):
        print "Motor Adjustment Data"
        #print self.motor_slope
        p = self.cm1.connectPeripheral(self.target1)
        c1 = p["293f365c-d247-4426-9ceb-a466378d457e"]["293f365c-d247-4426-9ceb-a466378d457e"] #Arduino UUID
        if (int(resistance_value) <0):
            resistance_value = 0
        elif (int(resistance_value) >= 100):
            resistance_value = 100
        data = [101,int(resistance_value)]
        print data[0]
        print data[1]
        byteexertime = bytearray(data)
        pprint(byteexertime)
        print c1.value
        c1.value = byteexertime
    
    def stop_system(self):
        print "STOP SYSTEM"
        self.unsubscribe_movesense()
        self.disconnect_arduino()
        self.timer1.stop()
        self.timer2.stop()
    
    def reset_System(self):
        self.timer1.stop()
        self.timer2.stop()
        self.timerhr.stop()
        self.timerecg.stop()
        self.unsubscribe_movesense()
        self.disconnect_arduino()
        self.newwindow = Care_App_InitDiag()
        self.newwindow.show()
        self.deleteLater()


    def unsubscribe_movesense(self):
        p = self.cm.connectPeripheral(self.target)
        self.c = p["1809"]["2A1C"] #Custom GATT UUID
        self.c.notify = False
        self.cm.disconnectPeripheral(p)

    def disconnect_arduino(self):
        p = self.cm1.connectPeripheral(self.target1)
        c1 = p["293f365c-d247-4426-9ceb-a466378d457e"]["293f365c-d247-4426-9ceb-a466378d457e"] #Arduino UUID
        self.cm1.disconnectPeripheral(p)
    
    def readarduino(self):
        p = self.cm1.connectPeripheral(self.target1)
        self.c1 = p["293f365c-d247-4426-9ceb-a466378d457e"]["293f365c-d247-4426-9ceb-a466378d457e"] #Arduino UUID
    
    
    class CustomWidgetHR(pg.GraphicsLayoutWidget):
            
        pg.setConfigOption('background', 'w')
        pg.setConfigOption('foreground', 'k')

    
        def __init__(self,duration,hr_rest,hr_target,parent=None,**kargs):
            #pg.GraphicsWindow.__init__(self)
            pg.GraphicsLayoutWidget.__init__(self)

        
            self.duration = duration
            self.hr_target = hr_target
            self.hr_rest = hr_rest
            self.dur_sec = self.duration * 60.0
            self.dur_sec_wu = math.floor(self.dur_sec / 3.0)
            self.dur_sec_cd = math.floor(self.dur_sec / 4.0)
            self.dur_sec_mp = self.dur_sec - self.dur_sec_wu - self.dur_sec_cd
            
            self.hr_xaxis = []
            self.hr_trapezoid = []
        
            p_hr = self.addPlot(labels = {'left':'Heart Rate (bpm)','bottom':'Time (min)'})      
            
            p_hr.setYRange(self.hr_rest - 10, self.hr_target + 10,padding = 0.1)  
            p_hr.showGrid(x=True,y=True,alpha=0.5)
            
            
            for i in range(int(self.dur_sec)):
                self.hr_xaxis.append(i/60.0)
                if (i < self.dur_sec_wu):
                    self.hr_trapezoid.append((((self.hr_target - self.hr_rest)/self.dur_sec_wu)*i) + self.hr_rest)
                elif ((i >= self.dur_sec_wu) and (i < (self.dur_sec_wu + self.dur_sec_mp))):
                    self.hr_trapezoid.append(self.hr_target)
                else:
                    self.hr_trapezoid.append(((-(self.hr_target - self.hr_rest)/self.dur_sec_wu)*(i - self.dur_sec_wu - self.dur_sec_mp)) + self.hr_target)

            self.curve_hr_trapezoid = p_hr.plot(self.hr_xaxis,self.hr_trapezoid,pen = pg.mkPen('g', width = 3, style = QtCore.Qt.DotLine))
      
            self.curve_hr = p_hr.plot(pen = pg.mkPen('r', width = 2))         
            self.ptrx = 0
            
            self.time_series_hr = np.linspace(0,0,int(self.dur_sec))        
            self.timerhr = pg.QtCore.QTimer(self)
            self.timerhr.timeout.connect(self.updateX)
            self.timerhr.start(1000)

        def plot(self,input_data):
            self.input_data = input_data

        def updateX(self):

                try:
                    if(self.ptrx < (self.duration*60)):
                        self.time_series_hr[self.ptrx] = self.input_data
                        self.curve_hr.setData(self.hr_xaxis,self.time_series_hr)

                        self.ptrx += 1
                    else:
                        self.timerhr.stop()
                        
                except ValueError:
                    pass

    class CustomWidgetECG(pg.GraphicsLayoutWidget):
        
        pg.setConfigOption('background', 'w')
        pg.setConfigOption('foreground', 'k')
        
        
        def __init__(self,duration,parent=None,**kargs):
            #pg.GraphicsWindow.__init__(self)
            pg.GraphicsLayoutWidget.__init__(self)
            
            
            self.duration = duration
            self.dur_sec = self.duration * 60.0
            self.dur_sec_wu = math.floor(self.dur_sec / 3.0) #warm up time
            self.dur_sec_cd = math.floor(self.dur_sec / 4.0) #cool down time
            self.dur_sec_mp = self.dur_sec - self.dur_sec_wu - self.dur_sec_cd #main phase time
            
            self.ecg_xaxis = np.linspace(0,0,1008)
            #self.hr_trapezoid = []
            
            #Initialze plot
            p_ecg = self.addPlot(labels = {'left':'ECG (mV)','bottom':'Time (milliseconds)'})
            
            p_ecg.setYRange(-3000,3000)
            p_ecg.showGrid(x=True,y=True,alpha=0.5)
            
            self.input_data_ecg = np.linspace(0,0,16) #initialize to 0
            
            self.ctr = 0 #declare window counter
            self.duration_ms = self.duration*60*1000
            
            
            self.curve_ecg = p_ecg.plot(pen = pg.mkPen('r', width = 2))
            self.ptrx_ecg = 0
            
            self.time_series_ecg = np.linspace(0,0,int(1008)) #initialize plot window
            
            #Update Interval
            self.timerecg = pg.QtCore.QTimer(self)
            self.timerecg.timeout.connect(self.updateX)
            self.timerecg.start(120)
                    
        def plot(self,input_data):
            j = 0
            while j<16:
                self.input_data_ecg[j] = input_data[j]
                j = j+1

        def updateX(self):
            try:
                if self.ctr<self.duration_ms:
                    if (self.ctr % 1008 == 0):
                        l=0
                        self.ecg_xaxis = np.linspace(0,0,1008)
                        for i in range(self.ctr,(self.ctr+1008)):
                            self.ecg_xaxis[l] = (i/125.0)*1000 #x-axis label
                            l += 1
                        self.ptrx_ecg = 0
                        self.clearplot()
                        self.ptrx_ecg = 0
                    k=0
                    self.ctr+=16
                    while k<16:
                        self.time_series_ecg[self.ptrx_ecg] = self.input_data_ecg[k]
                        self.input_data_ecg[k] = 0
                        self.curve_ecg.setData(self.ecg_xaxis,self.time_series_ecg) #set x-axis
                        self.ptrx_ecg += 1
                        k += 1
                else:
                    self.timerecg.stop()
            except ValueError:
                pass

        def clearplot(self):
            m = 0
            while m<1008:
                self.time_series_ecg[self.ptrx_ecg] = 0
                self.curve_ecg.setData(self.ecg_xaxis,self.time_series_ecg)
                self.ptrx_ecg += 1
                m += 1

def main():
    print("Start")

    app = QtGui.QApplication(sys.argv)
             
    dialog = Care_App_InitDiag()
    dialog.show()

    sys.exit(app.exec_())

    print("Program terminated")
    

    
if __name__ == "__main__":
    main()  
