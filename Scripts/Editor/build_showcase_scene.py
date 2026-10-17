"""Build and save the Step 5 scene using Unreal's existing editor Python support.

Run with UnrealEditor-Cmd ArcChain.uproject -run=pythonscript -script=<this file>.
The original map is backed up under Saved/ShowcaseBackup before the first save.
"""

import json
import math
from pathlib import Path
import shutil

import unreal


ROOT = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
MAP = "/Game/Maps/GameMap"
MATERIALS = "/Game/Showcase/Materials"
TAG = unreal.Name("ArcShowcaseDecor")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets = unreal.AssetToolsHelpers.get_asset_tools()
materials = unreal.MaterialEditingLibrary


def expression(material, cls, **properties):
    node = materials.create_material_expression(material, cls)
    for name, value in properties.items():
        node.set_editor_property(name, value)
    return node


def connect(source, destination, pin):
    # Unary material expressions expose their first pin without a display name.
    pin = {"Input": "", "AGreaterThanB": "A > B", "AEqualsB": "A == B", "ALessThanB": "A < B"}.get(pin, pin)
    assert materials.connect_material_expressions(source, "", destination, pin), (
        str(destination.get_class().get_name()), pin, materials.get_material_expression_input_names(destination))


def scalar(material, value):
    return expression(material, unreal.MaterialExpressionConstant, r=value)


def color(material, value):
    return expression(material, unreal.MaterialExpressionConstant3Vector,
                      constant=unreal.LinearColor(*value))


def surface(name, rgb, metallic, roughness, grid=False):
    path = MATERIALS + "/" + name
    material = unreal.load_asset(path)
    if material:
        return material
    material = assets.create_asset(name, MATERIALS, unreal.Material, unreal.MaterialFactoryNew())
    base = color(material, rgb)
    if grid:
        world = expression(material, unreal.MaterialExpressionWorldPosition)
        xy = expression(material, unreal.MaterialExpressionComponentMask, r=True, g=True)
        connect(world, xy, "Input")
        tile = expression(material, unreal.MaterialExpressionMultiply)
        connect(xy, tile, "A")
        connect(scalar(material, 1.0 / 300.0), tile, "B")
        fraction = expression(material, unreal.MaterialExpressionFrac)
        connect(tile, fraction, "Input")
        center = expression(material, unreal.MaterialExpressionSubtract)
        connect(fraction, center, "A")
        connect(scalar(material, 0.5), center, "B")
        absolute = expression(material, unreal.MaterialExpressionAbs)
        connect(center, absolute, "Input")
        x = expression(material, unreal.MaterialExpressionComponentMask, r=True)
        y = expression(material, unreal.MaterialExpressionComponentMask, g=True)
        connect(absolute, x, "Input")
        connect(absolute, y, "Input")
        maximum = expression(material, unreal.MaterialExpressionMax)
        connect(x, maximum, "A")
        connect(y, maximum, "B")
        seam = expression(material, unreal.MaterialExpressionIf)
        connect(maximum, seam, "A")
        connect(scalar(material, 0.495), seam, "B")
        for pin, value in [("AGreaterThanB", 1.0), ("AEqualsB", 1.0), ("ALessThanB", 0.0)]:
            connect(scalar(material, value), seam, pin)
        blend = expression(material, unreal.MaterialExpressionLinearInterpolate)
        connect(base, blend, "A")
        connect(color(material, (0.003, 0.006, 0.01)), blend, "B")
        connect(seam, blend, "Alpha")
        base = blend
    materials.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    materials.connect_material_property(scalar(material, metallic), "", unreal.MaterialProperty.MP_METALLIC)
    materials.connect_material_property(scalar(material, roughness), "", unreal.MaterialProperty.MP_ROUGHNESS)
    materials.layout_material_expressions(material)
    materials.recompile_material(material)
    assert unreal.EditorAssetLibrary.save_loaded_asset(material)
    return material


def decorate(actor, label, folder="Showcase/Architecture"):
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    actor.set_editor_property("tags", [TAG])
    return actor


def mesh(label, shape, position, scale, material, yaw=0.0):
    actor = decorate(actors.spawn_actor_from_class(unreal.StaticMeshActor,
        unreal.Vector(*position), unreal.Rotator(pitch=0.0, yaw=yaw, roll=0.0)), label)
    component = actor.static_mesh_component
    component.set_static_mesh(shapes[shape])
    component.set_material(0, material)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    return actor


def rect(label, position, target, lumens, tint, size, shadows=True):
    rotation = unreal.MathLibrary.find_look_at_rotation(unreal.Vector(*position), unreal.Vector(*target))
    actor = decorate(actors.spawn_actor_from_class(unreal.RectLight, unreal.Vector(*position), rotation),
                     label, "Showcase/Lighting")
    light = actor.get_component_by_class(unreal.RectLightComponent)
    light.set_mobility(unreal.ComponentMobility.MOVABLE)
    light.set_editor_property("intensity_units", unreal.LightUnits.LUMENS)
    light.set_intensity(lumens)
    light.set_light_color(unreal.LinearColor(*tint))
    light.set_editor_property("source_width", size[0])
    light.set_editor_property("source_height", size[1])
    light.set_attenuation_radius(2400.0)
    light.set_cast_shadows(shadows)
    light.set_editor_property("volumetric_scattering_intensity", 0.15)
    return actor


assert levels.load_level(MAP)
backup = ROOT / "Saved/ShowcaseBackup/GameMap-before-step5.umap"
backup.parent.mkdir(parents=True, exist_ok=True)
if not backup.exists():
    shutil.copy2(ROOT / "Content/Maps/GameMap.umap", backup)

existing = actors.get_all_level_actors()
conductors = sorted([a for a in existing if a.get_class().get_name() == "ArcConductor"],
                    key=lambda a: a.get_actor_label())
managers = [a for a in existing if a.get_class().get_name() == "ArcChainManager"]
assert conductors and len(managers) == 1, "Expected conductors and one existing chain manager."
assert len(conductors) <= 10, "Preserve existing conductors; review layouts with over ten nodes manually."

# Only replace template scenery and this script's own decor. Keep gameplay actors.
template_classes = {"DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog", "VolumetricCloud"}
for actor in existing:
    if (TAG in actor.get_editor_property("tags") or actor.get_class().get_name() in template_classes
            or actor.get_actor_label() in {"Floor", "SM_SkySphere"}):
        actors.destroy_actor(actor)

unreal.EditorAssetLibrary.make_directory(MATERIALS)
floor = surface("M_LabFloor", (0.025, 0.035, 0.047), 0.75, 0.34, grid=True)
body = surface("M_GraphiteMetal", (0.035, 0.048, 0.065), 0.7, 0.3)
steel = surface("M_ConductorSteel", (0.12, 0.16, 0.2), 0.8, 0.25)
accent = unreal.load_asset(MATERIALS + "/MI_CyanAccent")
if not accent:
    accent = assets.create_asset("MI_CyanAccent", MATERIALS,
                                 unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    materials.set_material_instance_parent(accent, unreal.load_asset("/Game/M_ConductorPulse"))
materials.set_material_instance_scalar_parameter_value(accent, "GlowIntensity", 8.0)
materials.set_material_instance_vector_parameter_value(accent, "GlowColor", unreal.LinearColor(0.1, 0.8, 1.0))
assert unreal.EditorAssetLibrary.save_loaded_asset(accent)

shapes = {name: unreal.load_asset("/Engine/BasicShapes/" + name) for name in ["Cube", "Cylinder", "Sphere"]}
assert all(shapes.values())

mesh("Lab / gridded metal deck", "Cube", (0, 0, -25), (32, 26, 0.5), floor)
mesh("Lab / rear wall", "Cube", (0, 1175, 350), (36, 0.6, 7), body)
for index, x in enumerate([-1500, -1000, -500, 0, 500, 1000, 1500]):
    mesh("Wall / rib %02d" % index, "Cube", (x, 1138, 350), (0.12, 0.14, 7), steel)
mesh("Wall / cyan horizon", "Cube", (0, 1137, 110), (30, 0.025, 0.025), accent)

for label, text, z, size in [("Wall / title", "ARCCHAIN", 340, 90),
                              ("Wall / subtitle", "ELECTRICAL SYSTEMS  /  FIELD LAB 01", 255, 20)]:
    sign = decorate(actors.spawn_actor_from_class(unreal.TextRenderActor,
        unreal.Vector(0, 1125, z), unreal.Rotator(pitch=0, yaw=-90, roll=0)), label)
    lettering = sign.get_component_by_class(unreal.TextRenderComponent)
    lettering.set_text(text)
    lettering.set_world_size(size)
    lettering.set_horizontal_alignment(unreal.HorizTextAligment.EHTA_CENTER)
    lettering.set_text_render_color(unreal.Color(r=140, g=195, b=220, a=255))
    lettering.set_cast_shadow(False)

# Interrupted perimeter strips leave the electrical network as the main source of motion.
for y in [-740, 740]:
    for x in [-840, -280, 280, 840]:
        mesh("Deck / guide %.0f %.0f" % (x, y), "Cube", (x, y, 0.8), (4.2, 0.018, 0.016), accent)
for x in [-1110, 1110]:
    for y in [-440, 0, 440]:
        mesh("Deck / edge %.0f %.0f" % (x, y), "Cube", (x, y, 0.8), (0.018, 2.8, 0.016), accent)

while len(conductors) < 10:
    # Spawn and copy just the gameplay settings; editor duplication expects a UI viewport.
    conductor = actors.spawn_actor_from_class(conductors[0].get_class(), unreal.Vector(0, 0, 100))
    assert conductor
    for property_name in ["glow_material", "glow_intensity", "pulse_duration", "glow_color",
                          "spark_system", "spark_count", "spark_color"]:
        conductor.set_editor_property(property_name, conductors[0].get_editor_property(property_name))
    conductors.append(conductor)

positions = []
for index, conductor in enumerate(conductors):
    angle = math.radians(18.0 + index * 36.0)
    x, y = 690.0 * math.cos(angle), 410.0 * math.sin(angle)
    shoulder_z = 18.0 + (index % 3) * 5.0
    top = shoulder_z + 36.0
    prefix = "Station %02d" % (index + 1)
    # Flattened spheres form soft rounded shoulders using built-in meshes only.
    mesh(prefix + " / rounded foot", "Sphere", (x, y, 11), (1.98, 1.98, 0.22), body)
    mesh(prefix + " / pedestal", "Cylinder", (x, y, shoulder_z + 12), (1.8, 1.8, 0.32), body)
    mesh(prefix + " / cyan collar", "Cylinder", (x, y, shoulder_z + 24), (1.82, 1.82, 0.025), accent)
    mesh(prefix + " / rounded cap", "Sphere", (x, y, top - 5), (1.74, 1.74, 0.1), steel)
    mesh(prefix + " / contact plate", "Cylinder", (x, y, top - 1), (1.6, 1.6, 0.02), steel)
    conductor.set_actor_label("Conductor %02d" % (index + 1))
    conductor.set_folder_path("Showcase/Conductors")
    conductor.set_actor_location(unreal.Vector(x, y, top + 45), False, False)
    conductor.set_actor_scale3d(unreal.Vector(0.9, 0.9, 0.9))
    conductor.set_actor_rotation(unreal.Rotator(pitch=0, yaw=math.degrees(angle) + 45, roll=0), False)
    conductor.get_component_by_class(unreal.StaticMeshComponent).set_material(0, steel)
    positions.append([x, y, top + 45])

manager = managers[0]
manager.set_folder_path("Showcase/Gameplay")
# Ten nodes require nine jumps; the existing limit of ten is already sufficient.
assert manager.get_editor_property("max_jumps") >= 9
radius = manager.get_editor_property("jump_radius")
for index in range(10):
    assert math.dist(positions[index], positions[(index + 1) % 10]) < radius

rect("Light / cool soft key", (-700, -300, 1050), (0, 0, 0), 24000,
     (0.65, 0.8, 1.0), (1000, 600))
rect("Light / blue front fill", (650, -900, 650), (0, 0, 80), 6000,
     (0.2, 0.55, 1.0), (1100, 500), shadows=False)
rect("Light / rear rim", (0, 950, 950), (0, 0, 50), 18000,
     (0.5, 0.85, 1.0), (1300, 350))

fog_actor = decorate(actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0)),
                     "Atmosphere / subtle blue haze", "Showcase/Lighting")
fog = fog_actor.get_component_by_class(unreal.ExponentialHeightFogComponent)
fog.set_fog_density(0.008)
fog.set_fog_height_falloff(0.25)
fog.set_fog_inscattering_color(unreal.LinearColor(0.012, 0.025, 0.045))
fog.set_fog_max_opacity(0.3)
fog.set_start_distance(600.0)
fog.set_volumetric_fog(True)
fog.set_volumetric_fog_distance(4500.0)

post = decorate(actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0)),
                "Grade / laboratory cinematic", "Showcase/Lighting")
post.set_editor_property("unbound", True)
post.set_editor_property("priority", 10.0)
settings = post.get_editor_property("settings")
for name, value in {
    "auto_exposure_method": unreal.AutoExposureMethod.AEM_HISTOGRAM,
    "auto_exposure_min_brightness": 3.0,
    "auto_exposure_max_brightness": 3.0,
    "auto_exposure_bias": 0.0,
    "bloom_intensity": 0.35,
    "bloom_threshold": 1.5,
    "vignette_intensity": 0.2,
    "color_contrast": unreal.Vector4(1.08, 1.08, 1.08, 1.0),
    "color_saturation": unreal.Vector4(0.9, 0.9, 0.9, 1.0),
    "motion_blur_amount": 0.0,
}.items():
    settings.set_editor_property("override_" + name, True)
    settings.set_editor_property(name, value)
post.set_editor_property("settings", settings)

camera_position = unreal.Vector(1550, -2100, 1850)
camera_rotation = unreal.MathLibrary.find_look_at_rotation(camera_position, unreal.Vector(0, 100, 90))
camera = decorate(actors.spawn_actor_from_class(unreal.CameraActor, camera_position, camera_rotation),
                  "Showcase Camera", "Showcase/Camera")
camera.set_editor_property("auto_activate_for_player", unreal.AutoReceiveInput.PLAYER0)
camera_component = camera.get_component_by_class(unreal.CameraComponent)
camera_component.set_field_of_view(42.0)
camera_component.set_editor_property("aspect_ratio", 16.0 / 9.0)
camera_component.set_editor_property("constrain_aspect_ratio", True)
camera_component.set_editor_property("post_process_blend_weight", 0.0)

for actor in actors.get_all_level_actors():
    if isinstance(actor, unreal.PlayerStart):
        actor.set_actor_location(unreal.Vector(1250, -1000, 140), False, False)
        actor.set_folder_path("Showcase/Gameplay")

assert unreal.EditorLoadingAndSavingUtils.save_current_level(), "Map save failed."
report = {"map": MAP, "saved": True, "conductors": len(conductors),
          "decor_actors": sum(TAG in a.get_editor_property("tags") for a in actors.get_all_level_actors()),
          "camera": str(camera_position), "positions": positions, "jump_radius": radius}
output = ROOT / "Saved/Diagnostics/showcase_build.json"
output.write_text(json.dumps(report, indent=2))
unreal.log("ARC_SHOWCASE_SAVED: " + json.dumps(report))
