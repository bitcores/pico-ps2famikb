import os
import evdev as ev
from selectors import DefaultSelector, EVENT_READ
from smbus2 import SMBus, i2c_msg

# configure i2c bus to use
i2cbus = 0
# ntsc or pal
pal = False

os.system('clear')

activekb = None
activemse = None
selector = DefaultSelector()

# start the mouse off at 0,0 for no particular reason
mousepos = [0,0]
# keep the state of mouse buttons
mousebtn = [0,0,0]
# vertical resolution references
ntscpal = (224,240)

keyboards = list()
mice = list()

devices = [ev.InputDevice(path) for path in ev.list_devices()]

for device in devices:
	c = device.capabilities()
	if 1 in c:
		# test if this is a mouse
		if 272 in c[1] and 273 in c[1]:
			mice.append(device)
		# test if this is a keyboard
		elif 14 in c[1] and 28 in c[1]:
			keyboards.append(device)

kbselect = False
mseselect = False
if len(keyboards) == 1:
	activekb = keyboards[0]
	kbselect = True
elif len(keyboards) > 1:
	print("Multiple keyboards detected")
	for i,k in enumerate(keyboards):
		print(i, ":", k.name)
	while not kbselect:
		g = input("Select keyboard: ")
		try:
			val = int(g)
			if val < 0:
				continue
			activekb = keyboards[val]
			kbselect = True
		except:
			kbselect = False
else:
	print("No keyboard attached")

if len(mice) == 1:
	activemse = mice[0]
	mseselect = True
elif len(mice) > 1:
	print("Multiple mice detected")
	for i,m in enumerate(mice):
		print(i, ":", m.name)
	while not mseselect:
		g = input("Select mouse: ")
		try:
			val = int(g)
			if val < 0:
				continue
			activemse = mice[val]
			mseselect = True
		except:
			mseselect = False
else:
	print("No mouse attached")

if kbselect:
	print("Active Keyboard: ", activekb.name)
	selector.register(activekb, EVENT_READ)

if mseselect:
	print("Active Mouse: ", activemse.name)
	selector.register(activemse, EVENT_READ)

if not kbselect and not mseselect:
	print("Can't continue without an input device")
	os._exit(0)

print("\nInitiating i2c connect")
# presuming using i2c bus 0 on Pi, addr is 0x17 -> 23
with SMBus(i2cbus) as bus:
	try:
		# use a write of all FF to reset pico message queue
		#bus.write_block_data(23,0,[255,255,255,255,255])

		print("pico-ps2famikb connected, taking control of input devices")
		
		while True:
			# first byte will be keyboard value, if present
			# https://forums.nesdev.org/viewtopic.php?p=248338#p248338
			msg = [0,0,0,0,0]
			for key, mask in selector.select():
				thisdev = key.fileobj
				for event in thisdev.read():
					et = event.type
					#print(et, type(et))
					try:
						if et == ev.ecodes.EV_KEY:
							print(ev.ecodes.keys[event.code], event.value)
							# check if mouse buttons first
							if "BTN_LEFT" in ev.ecodes.keys[event.code]:
								mousebtn[0] = event.value
							elif ev.ecodes.keys[event.code] == "BTN_MIDDLE":
								mousebtn[1] = event.value
							elif ev.ecodes.keys[event.code] == "BTN_RIGHT":
								mousebtn[2] = event.value
							
						elif et == ev.ecodes.EV_REL:
							#print(ev.ecodes.REL[event.code], event.value)
							if ev.ecodes.REL[event.code] == "REL_X":
								mousepos[0] = min(ntscpal[pal]-1, max(0, mousepos[0]+event.value))
							elif ev.ecodes.REL[event.code] == "REL_Y":
								mousepos[1] = min(255, max(0, mousepos[1]+event.value))
							print("mouse cursor at: ", mousepos, mousebtn)
					except:
						print(event)
						pass

				# let's build the mouse report
				msg[1] += mousebtn[0]
				msg[1] = msg[1] << 1
				msg[1] += mousebtn[2]
				msg[1] = msg[1] << 1
				msg[1] += mseselect
				msg[1] = msg[1] << 1
				msg[1] += kbselect
				msg[1] = msg[1] << 4
				msg[1] += 6 #0110
				msg[2] = mousepos[0]
				msg[3] = mousepos[1]
				# ignore forth byte for now
				print(msg)
				bus.write_block_data(23, 0, msg)

	except:
		print("pico-ps2famikb not found, check config and wiring")
		os._exit(0)

print("You should not be here")
os._exit(0)

