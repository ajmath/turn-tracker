#!/usr/bin/env python3
"""One CSG tree exports equivalent OpenSCAD source and Manifold STL meshes."""
from pathlib import Path
import json
import math
import zipfile
import numpy as np
import manifold3d as md
import trimesh
OUT = Path(__file__).resolve().parent
SEGMENTS = 128
class Shape:
    def __init__(self, mesh, code): self.mesh, self.code = mesh, code
    def move(self,x=0,y=0,z=0):
        return Shape(self.mesh.translate((x,y,z)), f'translate([{x},{y},{z}]){{{self.code}}}')
    def rotate(self,x=0,y=0,z=0):
        return Shape(self.mesh.rotate((x,y,z)), f'rotate([{x},{y},{z}]){{{self.code}}}')
    def __add__(self,b): return Shape(self.mesh+b.mesh, f'union(){{{self.code}{b.code}}}')
    def __sub__(self,b): return Shape(self.mesh-b.mesh, f'difference(){{{self.code}{b.code}}}')
    def __and__(self,b): return Shape(self.mesh^b.mesh, f'intersection(){{{self.code}{b.code}}}')
def box(x,y,z,at=(0,0,0)):
    return Shape(md.Manifold.cube((x,y,z)),f'cube([{x},{y},{z}]);').move(*at)
def cyl(r,h,at=(0,0,0)):
    return Shape(md.Manifold.cylinder(h,r,r,SEGMENTS),f'cylinder(r={r},h={h},$fn={SEGMENTS});').move(*at)
def combine(items):
    items=list(items); a=items[0]
    for b in items[1:]: a=a+b
    return a

def prism_xy(points,height,z=0):
    cross=md.CrossSection([points])
    return Shape(cross.extrude(height),f'linear_extrude(height={height}) polygon(points={points});').move(z=z)

def rounded_port(width,height,radius,depth,y,z):
    # Extruded rounded rectangle in the X/Z plane, along the cable axis Y.
    shapes=[box(width-2*radius,depth,height,(-width/2+radius,y,z-height/2)),
            box(width,depth,height-2*radius,(-width/2,y,z-height/2+radius))]
    for x in [-width/2+radius,width/2-radius]:
        for zz in [z-height/2+radius,z+height/2-radius]:
            shapes.append(cyl(radius,depth).rotate(x=-90).move(x=x,y=y,z=zz))
    return combine(shapes)

def reference(name):
    path=OUT/'reference'/f'{name}.stl'
    m=trimesh.load(path,force='mesh')
    a=md.Manifold(md.Mesh(np.array(m.vertices,dtype=np.float32),np.array(m.faces,dtype=np.uint32)))
    assert a.status()==md.Error.NoError
    return Shape(a,f'import("reference/{name}.stl");')

def design(option):
    large=option==1
    D,H=(90,45) if large else (70,38)
    R=D/2; t=2.4
    # Keep the complete circular profile. Only the USB pocket is locally inset.
    by=-23 if large else -9
    bd=10 if large else 25; bh=25 if large else 10
    pcbz=5.4
    front=R-3.8; rear=front-20.955
    hole=28.5 if large else 12; depth=33.3 if large else 17
    def profile(radius,height,z=0): return cyl(radius,height,(0,0,z))
    # Revolved quarter-circle top rim, radius 2.4 mm (32 arc segments).
    rounding=2.4
    outer=profile(R,H-rounding-t,t)
    for j in range(32):
        a=j*math.pi/64; b=(j+1)*math.pi/64
        z0=H-rounding+rounding*math.sin(a); z1=H-rounding+rounding*math.sin(b)
        r0=R-rounding+rounding*math.cos(a); r1=R-rounding+rounding*math.cos(b)
        cap=Shape(md.Manifold.cylinder(z1-z0,r0,r1,SEGMENTS),f'cylinder(h={z1-z0},r1={r0},r2={r1},$fn={SEGMENTS});').move(z=z0)
        outer=outer+cap
    shell=outer-profile(R-t,H-2*t+.1,t-.1)
    screw_points=[((R-6.5)*math.cos(math.radians(a)),(R-6.5)*math.sin(math.radians(a))) for a in (0,180,270)]
    for x,y in screw_points: shell=shell+cyl(4,H-2*t,(x,y,t))
    shell=shell-cyl((hole+.3)/2,t+.3,(0,0,H-t-.1))
    for x,y in screw_points: shell=shell-cyl(1.25,10,(x,y,t-.1))
    # Small recessed USB mouth, split horizontally between base and shell.
    usb_z=pcbz+2.11
    seam=usb_z
    lip_start=front-1
    lip=box(16,R-lip_start+1,12-t,(-8,lip_start,t)) & profile(R,H)
    shell=shell+lip
    # Remove the lower half for the matching upstand on the base.
    shell=shell-box(16.3,R-lip_start+2,seam-t+.15,(-8.15,lip_start-.15,t-.05))
    # Rounded socket mouth and a matching concentric recess: 1 mm border
    # on top, bottom and both sides, including where base meets shell.
    access=rounded_port(9.6,4.8,1.8,12,front-6,usb_z)
    recess=rounded_port(11.6,6.8,2.8,5,R-2.1,usb_z)
    # The rear of the socket has square mounting features; relieve these only
    # behind its nose, keeping the visible mouth tightly contoured.
    internal_usb=box(10,7.2,4.8,(-5,front-6,usb_z-2.4))
    access=access+internal_usb
    shell=shell-access-recess
    # Bottom-edge notch lets the shell drop over the glued-in slide switch.
    # Body lies directly on base, with actuator directed radially outward.
    switch_angle=45
    sx=R-t-6.6-2
    shell=shell-box(10,14,5.7,(R-8,-7,t-.1)).rotate(z=switch_angle)
    base=profile(R,t)
    clearance_radius = 1.6
    countersink_radius = 3.5
    # At 90 degrees included angle, the radius shrinks 1 mm per mm of depth.
    overcut = 0.1
    sink_depth = countersink_radius - clearance_radius
    countersink = Shape(
        md.Manifold.cylinder(sink_depth + overcut, countersink_radius + overcut,
                             clearance_radius, SEGMENTS),
        f'cylinder(h={sink_depth + overcut},r1={countersink_radius + overcut},'
        f'r2={clearance_radius},$fn={SEGMENTS});',
    )
    for x, y in screw_points:
        base = base - cyl(clearance_radius, t + 2 * overcut, (x, y, -overcut))
        base = base - countersink.move(x, y, -overcut)
    # Base upstand meets shell at USB center; it closes the old large notch.
    lower_lip=box(16,R-lip_start+1,seam-t,(-8,lip_start,t)) & profile(R,H)
    lower_lip=lower_lip-box(24,1.5,1.8,(-12,lip_start-.1,pcbz-.3))
    base=base+(lower_lip-access-recess)
    # Constant Y/Z section: rectangular spine and notched nose around PCB edge.
    # Keep the bar exterior, with a 1.5 mm high PCB-contact notch.
    section=[[-4.6,-1.1],[-1.8,-1.1],[-1.8,.1],[.15,-1.2],
             [.15,-.25],[-.35,-.25],[-.35,1.25],[.15,1.25],
             [.15,2.2],[-1.8,.9],[-1.8,2.1],[-4.6,2.1]]
    def section_bar(length,x,clearance=0):
        cross=md.CrossSection([section])
        if clearance: cross=cross.offset(clearance,circular_segments=64)
        code=f'polygon(points={section});'
        if clearance: code=f'offset(r={clearance},$fn=64){{{code}}}'
        shape=Shape(cross.extrude(length),f'linear_extrude(height={length}){{{code}}}')
        return shape.rotate(x=90,z=90).move(x=x,y=rear,z=pcbz)
    bar=section_bar(30,-15)
    # Confirm identical section area at both ends and the center.
    volumes=[(bar.mesh ^ box(1,16,16,(x,rear-12,pcbz-3)).mesh).volume() for x in [-14,0,13]]
    assert max(volumes)-min(volumes)<1e-5, 'Nonconstant bar section'
    socket=section_bar(40,-20,.2)
    # Open the cap over the committed cavity; keep its lower profile and walls.
    # Cutter overlaps the cavity so no zero-thickness roof fragments remain.
    roof_cut=box(40,3.01,10,(-20,rear-4.81,pcbz+.9))
    for sign in [-1,1]:
        x0=8.3 if sign>0 else -11
        rail=box(2.7,20.955,pcbz+2.1-t,(x0,rear,t-.1))
        post_x=8.3 if sign>0 else -14
        # Original front/height; add 1.2 mm only behind the existing rear wall.
        post=box(5.7,8.2,pcbz+3-t,(post_x,rear-6.6,t-.1))
        post=post-socket-roof_cut
        slope_probe=box(1,.4,.4,(post_x+1,rear-.8,pcbz+2.2))
        flat_probe=box(1,.5,.3,(post_x+1,rear-3.5,pcbz+2.5))
        assert (post.mesh ^ slope_probe.mesh).volume()>.1, 'Missing sloped retention roof'
        assert (post.mesh ^ flat_probe.mesh).volume()<1e-5, 'Flat roof must stay open'
        rail=rail+post
        rail=rail-box(18.58,30,1.5,(-9.29,rear-6.7,pcbz-.1))
        rail=rail-socket
        assert (rail.mesh ^ socket.mesh).volume()<1e-5, 'Changed socket interior'
        base=base+(rail & profile(R-t-.2,H))
    if large:
        # 8 degrees away from center: gravity loads the tape face and ledge.
        def battery_mount(shape): return shape.rotate(x=8).move(y=-28.3,z=t+1.2)
        panel=box(44,2,27,(-22,-2,-1))
        panel=panel+box(44,11.7,1.2,(-22,0,-1.2))
        panel=panel+box(44,1,3.7,(-22,10.7,-1.2))
        panel=battery_mount(panel)
        triangle=prism_xy([[-37,0],[-30.2,0],[-32.9,26]],2)
        for x in [-17,15]:
            panel=panel+triangle.rotate(x=90,z=90).move(x=x,z=t-.1)
        # Full-width solid infill from the base to the sloping shelf underside.
        # Top overlaps underside slightly for a single connected printed solid.
        fill=prism_xy([[-28.3,0],[-16.4,0],[-16.4,1.811],[-28.3,.138]],44)
        panel=panel+fill.rotate(x=90,z=90).move(x=-22,z=t-.1)
        base=base+panel
    # 10.6 mm clear switch pocket; move each 3 mm wall outward 0.1 mm.
    # Each has a short rear return; no platform or overhanging clamp.
    guides=combine([
        box(7.6,3,5.7,(sx-2.2,-8.3,t-.1)),
        box(3,4.2,5.7,(sx-2.2,-8.3,t-.1)),
        box(7.6,3,5.7,(sx-2.2,5.3,t-.1)),
        box(3,4.2,5.7,(sx-2.2,4.1,t-.1)),
    ]).rotate(z=switch_angle)
    base=base+guides
    refs={
        'battery':battery_mount(box(42,10,25,(-21,.3,0))) if large else box(42,25,10,(-22,by-12.5,t+.3)),
        'esp':reference('board').move(y=front,z=pcbz),
        'button':cyl(hole/2,depth,(0,0,H-t-depth)),
        'switch':box(5.6,10,5,(sx+1,-5,t)).rotate(z=switch_angle),
    }
    refs['usb']=reference('usb').move(y=front,z=pcbz)
    parts={'shell':shell,'base':base,'brace-bar':bar}
    known_battery_intersections=[]
    for name,a in parts.items():
        for ref,b in refs.items():
            overlap=(a.mesh^b.mesh).volume()
            # Option 2 battery placement is explicitly deferred by the user.
            if not large and ref=='battery':
                if overlap>1e-5: known_battery_intersections.append({'part':name,'overlap_mm3':round(overlap,3)})
                continue
            if overlap>1e-5: raise ValueError(f'Option {option}: {name} collides with {ref}: {overlap}')
    for i,(name,a) in enumerate(parts.items()):
        for name2,b in list(parts.items())[i+1:]:
            overlap=(a.mesh^b.mesh).volume()
            if overlap>1e-5: raise ValueError(f'Option {option}: {name} collides with {name2}: {overlap}')
    for i,(name,a) in enumerate(refs.items()):
        for name2,b in list(refs.items())[i+1:]:
            if {name,name2}=={'esp','usb'}: continue
            if not large and 'battery' in (name,name2): continue
            if (a.mesh^b.mesh).volume()>1e-5: raise ValueError(f'Component collision {option}: {name}/{name2}')
    # Check insertion with shell/bar/battery absent: install board before battery.
    for offset in np.linspace(-24,0,97):
        assert (base.mesh ^ refs['esp'].move(y=float(offset)).mesh).volume()<1e-5, f'Insertion obstruction option {option}, offset {offset}'
    for offset in np.linspace(-36,0,73):
        # Full rigid profile must translate through both matching sockets.
        moving=bar.move(x=float(offset)).mesh
        assert (base.mesh ^ moving).volume()<1e-5, f'Bar insertion obstruction {option}, {offset}'
        assert (refs['esp'].mesh ^ moving).volume()<1e-5, f'Bar/board insertion obstruction {option}'
    params={'switch_wall_thickness':3,'bar_back_wall_depth':2.8,'bar_back_wall_height':3.2,'pcb_channel_width':18.58,'pcb_channel_height':1.5,'pcb_contact_relief_per_face':.1,'front_stop_extra_clearance':.2,'switch_clear_width':10.6,'switch_arm_length':7.6,'switch_moved_outward':1,'bar_post_rear_wall_thickness':1.8,'bar_post_top_open':'flat section only','bar_post_roof':'sloped section retained','option_2_battery_placement_deferred':not large,'known_battery_intersections':known_battery_intersections,'battery_shelf_support':'full-width solid fill' if large else None,'bar_insertion_samples':73,'bar_insertion_check':'full rigid bar, including board notch, through both matching sockets','battery_tilt_degrees':8 if large else 0,'battery_bottom_catch':large,'battery_panel_size':[44,27,2] if large else None,'bar_slot_clearance':.2,'bar_profile':'constant extruded YZ section','pcb_notch_height':1.5,'bar_print_rotation_y':90,'switch_radial_inset_change':2,'battery_panel':large,'switch_cutout_height':5.6,'switch_guide_height':5.6,'usb_corner_radius':1.8,'usb_recess_border':1.0,'top_edge_radius':2.4,'usb_access_width':9.6,'usb_access_height':4.8,'insertion_samples':97,'diameter':D,'height':H,'usb_recess_back_y':front,'button_hole_nominal':hole,'button_hole_modeled':hole+.3,'battery_xyz':[42,bd,bh],'board_bottom_z':pcbz,'usb_opening_center_z':usb_z,'button_terminal_bottom_z':H-t-depth}
    return parts,refs,params

def mesh(shape):
    m=shape.mesh.to_mesh()
    return trimesh.Trimesh(vertices=np.asarray(m.vert_properties)[:,:3],faces=np.asarray(m.tri_verts),process=False)

def export_stl(shape,path,records):
    m=mesh(shape)
    assert m.is_watertight and m.is_winding_consistent and m.volume>0, path
    m.export(path)
    if 'REFERENCE' not in path.name:
        exported=trimesh.load(path,force='mesh')
        assert exported.is_watertight and exported.is_winding_consistent, path
    records[path.name]={'watertight':bool(m.is_watertight),'triangles':len(m.faces),'bounds_mm':m.bounds.round(3).tolist(),'volume_mm3':round(m.volume,3),'bytes':path.stat().st_size}

if __name__=='__main__':
    (OUT/'stl').mkdir(exist_ok=True)
    records={}; data={}; designs=[]
    scad=['// Generated by generate.py: exact same CSG tree as the verified STL exports.\n// Adjust dimensions in generate.py and regenerate to keep both formats equivalent.\noption=1; part="assembly";\n']
    for option in [1,2]:
        parts,refs,params=design(option); designs.append((parts,refs,params)); data[f'option-{option}']=params
        for name,s in {**parts,**{f'reference-{k}':v for k,v in refs.items()}}.items():
            module=f'o{option}_{name.replace("-","_")}'
            scad.append(f'module {module}(){{{s.code}}}\n')
        printable={}
        for name,s in parts.items():
            p=s.rotate(x=180).move(z=params['height']) if name=='shell' else s
            if name=='brace-bar': p=s.rotate(y=90).move(z=15)
            printable[name]=p
            export_stl(p,OUT/'stl'/f'option-{option}-{name}.stl',records)
        # Exploded view arranged on one workplane for easy Tinkercad inspection.
        spread=parts['base']+parts['shell'].rotate(x=180).move(x=params['diameter']+12,z=params['height'])
        spread=spread+printable['brace-bar'].move(x=-params['diameter']/2-24)
        export_stl(spread,OUT/'stl'/f'option-{option}-exploded.stl',records)
        # Reference layout omits shell and includes separate reference solids.
        export_stl(combine([parts['base'],*refs.values()]),OUT/'stl'/f'option-{option}-component-layout-REFERENCE.stl',records)
        scad.append(f'if(option=={option}){{\n')
        for name in parts:
            module=f'o{option}_{name.replace("-","_")}'
            transform=f'translate([0,0,{params["height"]}]) rotate([180,0,0]) ' if name=='shell' else ''
            if name=='brace-bar': transform='translate([0,0,15]) rotate([0,90,0]) '
            scad.append(f'if(part=="{name}") {transform}{module}();\n')
        scad.append(f'if(part=="assembly"||part=="layout"){{\nif(part=="assembly") color([.8,.8,.85,.3]) o{option}_shell();\no{option}_base();\n')
        for ref,color in [('battery','[.25,.55,.95,.8]'),('esp','[.1,.65,.3]'),('button','[.95,.55,.1,.6]'),('switch','[.2,.2,.2]'),('usb','[.6,.6,.6]')]:
            scad.append(f'color({color}) o{option}_reference_{ref}();\n')
        scad.append(f'color([.4,.6,.8]) o{option}_brace_bar();\n}}\n}}\n')
        (OUT/f'option-{option}.scad').write_text(f'// Option {option}: centered button, {params["diameter"]} mm circular puck with local USB recess.\ninclude <puck.scad>\noption={option};\n')
    (OUT/'puck.scad').write_text(''.join(scad))
    (OUT/'validation.json').write_text(json.dumps({'parameters':data,'checks':'Part/part and component/part intersections checked; option 2 battery placement explicitly deferred with known intersections recorded in parameters. All meshes watertight with consistent winding.','files':records},indent=2)+'\n')
    # A consistent preview of the actual exported mesh geometry.
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection
    fig=plt.figure(figsize=(14,10),facecolor='#f5f4f0')
    for i,(parts,refs,params) in enumerate(designs):
        for row in [0,1]:
            ax=fig.add_subplot(2,2,row*2+i+1,projection='3d')
            items=([(parts['base'],'#b7bdc3'),(parts['shell'].rotate(x=180).move(x=params['diameter']+12,z=params['height']),'#c5cbd2')] if row==0 else [(parts['base'],'#c6cbd0'),(refs['battery'],'#6ca1e9'),(refs['esp'],'#54ae78'),(refs['button'],'#edb15d'),(refs['switch'],'#555555'),(parts['brace-bar'],'#82b2cd')])
            bounds=[]; triangles=[]; facecolors=[]
            from matplotlib.colors import to_rgba
            for shape,color in items:
                m=mesh(shape);bounds.extend(m.bounds)
                triangles.extend(m.triangles); facecolors.extend([to_rgba(color)]*len(m.faces))
            poly=Poly3DCollection(np.array(triangles),facecolors=facecolors,edgecolors=None,shade=True)
            ax.add_collection3d(poly)
            b=np.array(bounds);lo=b.min(axis=0);hi=b.max(axis=0);c=(lo+hi)/2;span=max(hi-lo)*.54
            ax.set_xlim(c[0]-span,c[0]+span);ax.set_ylim(c[1]-span*.65,c[1]+span*.65);ax.set_zlim(0,max(params['height'],hi[2])+3)
            ax.set_box_aspect((2*span,1.3*span,params['height']+3));ax.view_init(elev=50,azim=45)
            ax.set_axis_off();ax.set_facecolor('#f5f4f0')
            ax.set_title(f'Option {i+1} · Ø{params["diameter"]} × {params["height"]} mm\n'+('Base and inverted shell' if row==0 else 'Component layout — shell hidden'),fontsize=13)
    fig.suptitle('Centered-button turn tracker enclosures',fontsize=20)
    fig.text(.5,.015,'Blue: battery  •  Green: ESP board  •  Orange: provisional button envelope',ha='center',fontsize=12)
    fig.tight_layout(rect=(0,.04,1,.94));fig.savefig(OUT/'preview.png',dpi=150)
    # Close-up views of the actual mesh, for reviewing the changed interfaces.
    parts,refs,params=designs[0]; R=params['diameter']/2; front=params['usb_recess_back_y']; rear=front-20.955
    detail=plt.figure(figsize=(11,5),facecolor='#f5f4f0')
    for j in [0,1]:
        ax=detail.add_subplot(1,2,j+1,projection='3d'); tris=[]; colors=[]
        clip=box(29,30,14,(-14.5,rear-5,0)) if j==0 else box(22,10,14,(-11,front-2,0))
        items=[(parts['base'],'#b7bdc3')] if j==0 else [(parts['base'],'#85a7bc'),(parts['shell'],'#c9cdd2')]
        for shape,color in items:
            m=mesh(shape & clip); tris.extend(m.triangles);colors.extend([to_rgba(color)]*len(m.faces))
        ax.add_collection3d(Poly3DCollection(np.array(tris),facecolors=colors,shade=True))
        ax.set_xlim(-14,14);ax.set_ylim(rear-5,R+1);ax.set_zlim(0,14)
        ax.set_box_aspect((28,R-rear+6,14));ax.view_init(elev=38 if j==0 else 0,azim=110 if j==0 else 90)
        ax.set_axis_off();ax.set_facecolor('#f5f4f0')
        ax.set_title('Bar posts · sloped roof retained, flat roof open' if j==0 else 'USB opening · base and shell assembled',fontsize=11)
    detail.tight_layout();detail.savefig(OUT/'interface-details.png',dpi=170)
    # End-on section: actual bar (red) and supplied PCB (green).
    figbar,axb=plt.subplots(figsize=(7,4),facecolor='#f5f4f0')
    bm=mesh(designs[0][0]['brace-bar']); keep=bm.face_normals[:,0]>.99
    from matplotlib.collections import PolyCollection
    axb.add_collection(PolyCollection(bm.triangles[keep][:,:,[1,2]],facecolors='#cf4d57',edgecolors='none'))
    rr=designs[0][2]['usb_recess_back_y']-20.955; zz=designs[0][2]['board_bottom_z']
    axb.add_patch(plt.Rectangle((rr,zz),3,1,facecolor='#54ae78'))
    axb.set_xlim(rr-11,rr+2.5);axb.set_ylim(zz-1.8,zz+2.8)
    axb.set_aspect('equal');axb.axis('off');axb.set_title('Bar section · relaxed PCB notch\nGreen: PCB rear edge',fontsize=12)
    figbar.tight_layout();figbar.savefig(OUT/'brace-bar-detail.png',dpi=170)
    with zipfile.ZipFile(OUT/'tinkercad-enclosures.zip','w',zipfile.ZIP_DEFLATED) as z:
        for name in sorted(records): z.write(OUT/'stl'/name,'stl/'+name)
        for p in (OUT/'reference').glob('*'): z.write(p,'reference/'+p.name)
        for name in ['README.md','brace-bar-detail.png','interface-details.png','preview.png','validation.json','puck.scad','option-1.scad','option-2.scad','generate.py','requirements-cad.txt']:
            z.write(OUT/name,name)
    with zipfile.ZipFile(OUT/'tinkercad-enclosures.zip') as archive:
        assert archive.testzip() is None, 'Corrupt export archive'
        for name in records:
            assert archive.read('stl/'+name)==(OUT/'stl'/name).read_bytes()
    print(json.dumps({'parameters':data,'STL_exports':len(records),'max_STL_bytes':max(v['bytes'] for v in records.values())},indent=2))
