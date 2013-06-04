import Image, sys

def image_to_greyscale(file_name):
  name, ext = file_name.split('.')
  Image.open(file_name).convert('L').save(name + '_g.' + ext)

def image_to_txt(file_name):
  name, ext = file_name.split('.')
  f = open(name + '.txt', 'w')
  img = Image.open(file_name).convert('L')
  f.write('%d %d\n' % (img.size[1], img.size[0]))
  for y in range(img.size[1]):
    for x in range(img.size[0]):
      f.write('%d ' % img.getpixel((x, y)))
    f.write('\n')

def txt_to_image(file_name):
  name, ext = file_name.split('.')
  f = open(file_name, 'r')
  height, width = f.readline().strip().split()
  height, width = int(height), int(width)
  img = Image.new('L', (width, height))
  y = 0
  for line in f:
    x = 0
    pixels = line.strip().split()
    for pixel in pixels:
      img.putpixel((x, y), int(pixel))
      x = x + 1
    y = y + 1
  img.save(name + '_filtered.jpg')
       
if __name__ == '__main__':
  if len(sys.argv) < 3:
    print 'Usage: python image_filter.py option input_file'
    sys.exit(0)

  option = sys.argv[1]
  if option == '-g':
    image_to_greyscale(sys.argv[2])
  elif option == '-i':
    image_to_txt(sys.argv[2])
  elif option == '-o':
    txt_to_image(sys.argv[2])
