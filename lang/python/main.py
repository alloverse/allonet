from cffi import FFI
import allonet

if __name__ == "__main__":
	client = allonet.Client(False)

	client.connect('alloplace://localhost', {"display_name": "~>^~~~~~"}, open("avatar.json", "r").read())

	p = 0.1
	while True:
		client.intent(xmovement=p)
		client.poll(1)
