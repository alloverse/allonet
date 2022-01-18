from cffi import FFI
import allonet

if __name__ == "__main__":
	client = allonet.Client(False)

	client.connect('alloplace://localhost', '{"display_name": "~>^~~~~~"}', open("avatar.json", "r").read())

	while True:
		client.poll(100)
