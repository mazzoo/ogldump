__author__ = 'Matthias Wenzel'
__version__ = '0.1 June 2009'
__url__ = ["http://mazzoo.de/blog/"]
__email__ = ["Matthias Wenzel, reprap:mazzoo*de", "scripts"]
__bpydoc__ = """
this script renders multiple .STL files to a html page

invoke like this:
RENDER_STL="`echo /dir/to/drawelements_*.stl`" blender -P render_stl.py
"""

import os
import Blender
from Blender import Scene
from Blender import Constraint
from Blender.Constraint import Type
from Blender.Constraint import Settings
from Blender.Scene import Render
from Blender import *


pics_per_row = 5


def unlink_all(oname):
	objs = Blender.Object.Get()
	for ob in objs:
		if str(ob.getName())[0:len(oname)] == oname:
			scn.objects.unlink(ob)
			scn.update(0)


def print_all_obj():
	objs = Blender.Object.Get()
	print "all objects: " + str(objs)


def print_sel_obj():
	objs = Blender.Object.GetSelected()
	print "sel objects: " + str(objs)


def html_write_head(fh):
	fh.write("<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\"\n")
	fh.write("\"http://www.w3.org/TR/html4/loose.dtd\">\n")
	fh.write("<html><head><title>some stl files</title>\n")
	fh.write("<meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\">\n")
	fh.write("</head><body bgcolor=\"#222222\">\n")
	fh.write("<table>\n")
	fh.write("<tr>\n")


def html_write_table_entry(fh, fname, count, ext):
	fh.write("<td>")
	fh.write("<font color=\"#aaaaaa\">\n")
	fh.write("<img src=\"" + fname + ext + ".png\" alt=\"" + fname + ext + ".png\">")
	fh.write("<br>\n")
	fh.write("[" + str(count) + "] <a href=\"" +  fname + "\">" + fname.split("/")[-1] + "</a>")
	fh.write("</font>")
	fh.write("</td>\n")
	if (count % pics_per_row) == 0:
		fh.write("</tr>\n")
		fh.write("<tr>\n")


def html_write_foot(fh):
	fh.write("</tr>")
	fh.write("</table>")
	fh.write("</body>")
	fh.write("</html>")



print "start"
scn = Scene.GetCurrent()

# remove the Cube
unlink_all("Cube")

# html
html_norm = open("stl.html", "w")
html_zoom = open("stl_zoom.html", "w")
html_write_head(html_norm)
html_write_head(html_zoom)



objs = Blender.Object.Get()
for ob in objs:
	if str(ob.getName())[0:6] == 'Camera':
		c = ob.constraints.append(Type.TRACKTO)


stl_count = 0

stl_all = os.getenv('RENDER_STL').split()
for stl_name in stl_all:

	# delete old Mesh
	unlink_all("Mesh")

	print "loading " + stl_name + " :)" 
	Blender.Load(stl_name)

	objs = Blender.Object.Get()
	for ob in objs:
		if str(ob.getName())[0:4] == 'Mesh':
			ob.select(True)
			Blender.Redraw()

	# render normal
	ctx = scn.getRenderingContext()
	Render.EnableDispView()
	ctx.sizePreset(Render.PREVIEW)
	ctx.imageType = Render.PNG
	ctx.render()
	ctx.setRenderPath("")
	ctx.saveRenderedImage(stl_name + ".png", 0)

	print_sel_obj()

	objs = Blender.Object.Get()
	for ob in objs:
		if str(ob.getName())[0:6] == 'Camera':
			# add constraint
			#ob.constraints.append(Constraint.TRACKTO)
			#ob.constraints.append(TRACKTO)
			#ob.constraints.Settings = TRACKTO
			#c = ob.constraints.append(Type.TRACKTO)
			c[Settings.TARGET] = Blender.Object.GetSelected()[0]
			c[Settings.UP]     = Settings.UPY
			c[Settings.TRACK]  = Settings.TRACKNEGZ

	print_sel_obj()

	cam = Blender.Object.Get('Camera')
	for c in cam.constraints:
		print c

	# html
	stl_count = stl_count + 1
	html_write_table_entry(html_norm, stl_name, stl_count, "")
	html_write_table_entry(html_zoom, stl_name, stl_count, "_zoom")


html_write_foot(html_norm)
html_write_foot(html_zoom)
html_norm.close()
html_zoom.close()

print "stop"
#Blender.Quit()

