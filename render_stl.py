__author__ = 'Matthias Wenzel'
__version__ = '0.1 June 2009'
__url__ = ["http://mazzoo.de/blog/"]
__email__ = ["Matthias Wenzel, reprap:mazzoo*de", "scripts"]
__bpydoc__ = """
this script renders multiple .STL files to a html page

invoke like this:
blender -P render_stl.py
"""

import os
import Blender
from Blender import Scene
from Blender.Scene import Render

pics_per_row = 5

def del_all_meshes():
	objs = Blender.Object.Get()
	for ob in objs:
		if str(ob.getName())[0:4] == 'Mesh':
			scn.objects.unlink(ob)
			scn.update(0)
#			print "deleted " + str(ob.getName())
#		else:
#			print "keeping " + str(ob.getName())




print "start"

# remove the Cube
scn = Scene.GetCurrent()
cube = Blender.Object.Get("Cube")
scn.objects.unlink(cube)
scn.update(0)

# html
html = open("stl.html", "w")
html.write("<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\n")
html.write("\"http://www.w3.org/TR/html4/loose.dtd\">\n")
html.write("<html><head><title>some stl files</title>\n")
html.write("<meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\">\n")
html.write("</head><body bgcolor=\"#222222\">\n")
html.write("<table>\n")
html.write("<tr>\n")

stl_count = 0

stl_all = os.getenv('RENDER_STL').split()
for stl_name in stl_all:

	print "loading " + stl_name + " :)" 
	Blender.Load(stl_name)

	# render
	ctx = scn.getRenderingContext()
	Render.EnableDispView()
	ctx.sizePreset(Render.PREVIEW)
	ctx.imageType = Render.PNG
	ctx.render()
	ctx.setRenderPath("")
	ctx.saveRenderedImage(stl_name + ".png", 0)

	# delete old Mesh
	del_all_meshes()

	# html
	html.write("<td>")
	html.write("<font color=\"#aaaaaa\">\n")
	html.write("<img src=\"" + stl_name + ".png\" alt=\"" + stl_name + ".png\">")
	html.write("<br>\n")
	html.write("[" + str(stl_count) + "] <a href=\"" +  stl_name + "\">" + stl_name.split("/")[-1] + "</a>")
	html.write("</font>")
	html.write("</td>\n")
	stl_count = stl_count + 1
	if (stl_count % pics_per_row) == 0:
		html.write("</tr>\n")
		html.write("<tr>\n")

#print str(ctx.fps) + " fps"
#print ctx.cFrame

html.write("</tr>")
html.write("</table>")
html.write("</body>")
html.write("</html>")
html.close()

print "stop"
Blender.Quit()

