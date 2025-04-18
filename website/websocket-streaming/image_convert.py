import numpy as np
from PIL import Image

width, height = 160, 120

with open("received_image.raw", "rb") as f:
    raw_data = f.read()

#this creates a numpy array from raw bytes
img_array = np.frombuffer(raw_data, dtype=np.uint8).reshape((height, width))

#this converts pillow image (mode L means 8 bit grayscale)
img = Image.fromarray(img_array, mode="L")

#save image
img.save("output.png")
#img.show()
