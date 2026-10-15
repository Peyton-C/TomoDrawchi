# esptool wrapper: ROM loader with small flash blocks (CH343 + macOS CDC driver mangles big writes)
import sys, esptool
from esptool.loader import ESPLoader
BLOCK = int(sys.argv[1], 0)
ESPLoader.FLASH_WRITE_SIZE = BLOCK
for cls in ESPLoader.__subclasses__():
    for c in [cls] + cls.__subclasses__():
        if "FLASH_WRITE_SIZE" in c.__dict__: c.FLASH_WRITE_SIZE = BLOCK
esptool.main(sys.argv[2:])
