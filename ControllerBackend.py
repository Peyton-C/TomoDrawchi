import serial
import time

class Controller:
    def __init__(self, BAUD, PORT):
        time.sleep(1)
        self.__SERIAL_INSTANCE = serial.Serial(PORT, BAUD, timeout=1)
    
    def __write(self, cmd):
        self.__SERIAL_INSTANCE.write(((cmd.strip()) + "\n").encode())

    def A(self):
        self.__write("1")
    def A_HOLD(self):
        self.__write("h1")
    def A_RELEASE(self):
        self.__write("r1")

    def B(self):
        self.__write("2")
    def B_HOLD(self):
        self.__write("h2")
    def B_RELEASE(self):
        self.__write("r2")

    def X(self):
        self.__write("3")
    def X_HOLD(self):
        self.__write("h3")
    def X_RELASE(self):
        self.__write("r3")

    def Y(self):
        self.__write("4")
    def Y_HOLD(self):
        self.__write("h4")
    def Y_RELEASE(self):
        self.__write("r4")


    def UP(self):
        self.__write("5")
    def DOWN(self):
        self.__write("6")
    def LEFT(self):
        self.__write("7")
    def RIGHT(self):
        self.__write("8")
    
    def PLUS(self):
        self.__write("9")
    def MINUS(self):
        self.__write("10")
    
    def L3(self):
        self.__write("12")
    def R(self):
        self.__write("11")
