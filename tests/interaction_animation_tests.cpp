#include "interaction/Interaction.hpp"
#include "animation/Animation.hpp"
#include "camera/Camera.hpp"
#include "assets/AssetManager.hpp"
#include "world/InteractionAnimationDemo.hpp"
#include "world/InteractionAnimationDemoSettings.hpp"
#include "world/DemoSceneSettings.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace ecosphere;
void check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
bool near(float a, float b) { return std::abs(a-b)<0.0002f; }
bool near(glm::vec3 a, glm::vec3 b) { return glm::length(a-b)<0.0002f; }
bool near(const glm::mat4& a, const glm::mat4& b) {
    for (int c=0; c<4; ++c) for (int r=0; r<4; ++r) if (!near(a[c][r],b[c][r])) return false;
    return true;
}
template<class Exception=std::invalid_argument, class Action> void rejects(Action action) {
    try { action(); } catch (const Exception&) { return; }
    throw std::runtime_error("Expected rejection");
}
InteractiveObject item(const char* id, glm::vec3 position = {0,0,0}) {
    InteractiveObject object; object.id=id; object.base.position=position; return object;
}
void picking() {
    const Aabb box;
    const Ray ray{{0,0,3},{0,0,-1}};
    check(near(*intersectAabb(ray,box),2.5f),"Front face distance");
    check(!intersectAabb({{2,0,3},{0,0,-1}},box),"Parallel ray outside slab");
    check(!intersectAabb({{0,0,3},{0,0,1}},box),"Box behind ray");
    check(near(*intersectAabb({{0,0,0},{1,0,0}},box),0),"Ray starts inside");
    check(intersectAabb({{0.5f,0.5f,3},{0,0,-1}},box).has_value(),"Grazing corner");
    Pose placement{{0,0,-2},glm::angleAxis(glm::radians(90.0f),glm::vec3(0,1,0)),{2,3,4}};
    check(near(*intersectAabb(ray,box,placement.matrix()),4),"Nonuniform rotated box world distance");
    rejects([&] { intersectAabb({{0,0,0},{0,0,0}},box); });
    rejects([&] { intersectAabb(ray,{{1,0,0},{0,1,1}}); });
    rejects([&] { intersectAabb(ray,box,glm::mat4(0)); });
    const float nan=std::numeric_limits<float>::quiet_NaN();
    rejects([&] { intersectAabb({{nan,0,0},{0,0,1}},box); });
    Camera camera; camera.position={0,0,3}; camera.pitchDegrees=0;
    const auto projection=*camera.projection(800,600);
    const auto center=cameraRay(camera.view(),projection);
    check(near(center.direction,{0,0,-1}) && near(center.origin.z,2.9f),"Camera center near plane ray");
    check(cameraRay(camera.view(),projection,{1,1}).direction.x>0,"NDC corner direction");
    rejects([&] { cameraRay(camera.view(),projection,{2,0}); });
    rejects([&] { cameraRay(glm::mat4(0),projection); });
    InteractionSystem system; system.add(item("far",{0,0,-2})); system.add(item("near"));
    check(system.pick(ray)==std::optional<std::string>("near"),"Nearest object independent of registration order");
    check(system.pick({{5,0,3},{0,0,-1}})==std::nullopt && !system.selected(),"Miss clears selection");
    check(!system.select("absent") && !system.selected(),"Unknown selection");
    rejects<std::out_of_range>([&] { system.object("absent"); });
    rejects([&] { system.add(item("far")); });
    rejects([&] { system.add(item("")); });
    auto orphan=item("orphan"); orphan.parent="absent";
    rejects<std::out_of_range>([&] { system.add(orphan); });
    check(system.objects().size()==2,"Failed registration is atomic");
    system.select("near"); system.take(ray,2); check(system.holding(),"Take");
    check(near(glm::vec3(system.world("near")[3]),{0,0,1}),"Held target on ray");
    system.follow({{1,0,3},{0,0,-1}});
    check(near(glm::vec3(system.world("near")[3]),{1,0,1}),"Held object follows camera ray");
    system.drop(); auto before=system.world("near"); system.follow(ray);
    check(near(before,system.world("near")),"Drop stops following");
    system.manipulate({1,2,3},glm::angleAxis(glm::radians(90.0f),glm::vec3(0,1,0)),2);
    const auto& m=system.object("near").manipulation;
    check(near(m.position,{2,2,4}) && near(m.scale,{2,2,2}),"Translation and scale applied");
    check(near(glm::vec3(glm::mat4_cast(m.orientation)*glm::vec4(1,0,0,0)),{0,0,-1}),"Manipulated rotation");
    check(near(glm::vec3(system.world("near")[3]),{2,2,4}),"Rotate/scale preserve object center");
    before=system.world("near"); rejects([&] { system.manipulate({},glm::quat(1,0,0,0),0); });
    check(near(before,system.world("near")),"Invalid manipulation is atomic");
    rejects([&] { system.take(ray,-1); });
    system.deselect(); check(!system.holding() && !system.selected(),"Deselect releases");
    system.manipulate({1,0,0},glm::quat(1,0,0,0),1);
    check(near(before,system.world("near")),"No selection means no mutation");
    std::cout << "Ray/AABB: hit/miss/inside/parallel/corner/nonuniform, camera unprojection, nearest ID, manipulation/take/drop PASS\n";
}
void hierarchy() {
    InteractionSystem system;
    auto parent=item("parent",{10,0,0}); parent.base.orientation=glm::angleAxis(glm::radians(90.0f),glm::vec3(0,1,0)); parent.base.scale={2,2,2};
    system.add(parent); auto child=item("child",{1,0,0}); child.parent="parent"; system.add(child);
    check(near(glm::vec3(system.world("child")[3]),{10,0,-2}),"Parent rotation/scale reaches child");
    system.object("parent").animation.position.y=1;
    check(near(glm::vec3(system.world("child")[3]),{10,2,-2}),"Animated hierarchy");
    system.select("child"); system.take({{0,0,3},{0,0,-1}},2);
    check(near(glm::vec3(system.world("child")[3]),{0,0,1}),"Taking child uses inverse parent");
    check(near(system.object("child").base.position,{1,0,0}),"Base is preserved");
    system.object("parent").parent="child";
    rejects([&] { system.world("child"); });
    std::cout << "Parent/child matrices, animation composition, child manipulation and cycle rejection PASS\n";
}
void animation() {
    AnimationPlayer a(8), b(8);
    for (int i=0;i<30;++i) a.update(1.0/30);
    for (int i=0;i<144;++i) b.update(1.0/144);
    check(near(float(a.time()),float(b.time())),"Time independent of FPS");
    SimpleAnimation spin{SimpleMotion::rotation,{0,1,0},90,0};
    check(near(spin.sample(a.time()).matrix(),spin.sample(b.time()).matrix()),"Rotation independent of FPS");
    SimpleAnimation linear{SimpleMotion::periodicTranslation,{1,0,0},2,0.5};
    check(near(linear.sample(1).position,{2,0,0}) && near(linear.sample(2).position,{}),"Periodic triangle wave");
    SimpleAnimation oscillation{SimpleMotion::oscillation,{0,0,1},30,1};
    const auto swung=oscillation.sample(0.25);
    check(near(glm::vec3(swung.matrix()*glm::vec4(1,0,0,0)),{std::cos(glm::radians(30.0f)),0.5f,0}),"Simple angular oscillation");
    a.setPlaying(false); const double paused=a.time(); a.update(5); check(a.time()==paused,"Pause");
    a.setPlaying(true); a.setSpeed(2); a.update(0.5); check(near(float(a.time()),2),"Resume and speed");
    a.restart(); check(a.time()==0 && a.playing(),"Restart");
    a.update(4.25); check(near(float(a.time()),0.5f),"Loop retains remainder");
    check(near(float(a.elapsed()),8.5f),"Continuous time survives keyframe repeat");
    a.setSpeed(0); a.update(5); check(near(float(a.time()),0.5f),"Zero speed freezes");
    AnimationPlayer once(1,false); once.update(2); check(once.time()==1 && !once.playing(),"Nonrepeat clamps and stops");
    once.restart(); once.update(0); check(once.time()==0,"Zero delta");
    Pose start, end; end.position={2,4,6}; end.scale={3,5,7};
    end.orientation=glm::angleAxis(glm::radians(120.0f),glm::vec3(0,1,0))*2.0f;
    KeyframeTrack track({{0,start},{2,end}});
    const auto middle=track.sample(1);
    check(near(middle.position,{1,2,3}) && near(middle.scale,{2,3,4}),"Position/scale lerp");
    check(near(glm::length(middle.orientation),1),"Normalized SLERP");
    check(near(glm::vec3(glm::mat4_cast(middle.orientation)*glm::vec4(1,0,0,0)),{0.5f,0,-std::sqrt(3.0f)/2}),"SLERP midpoint orientation");
    check(near(track.sample(-5).position,start.position) && near(track.sample(99).position,end.position),"Out of range samples clamp");
    check(near(track.sample(2).scale,end.scale),"Exact last key");
    Pose opposite=start; opposite.orientation=-start.orientation;
    check(near(KeyframeTrack({{0,start},{1,opposite}}).sample(0.5).matrix(),glm::mat4(1)),"Antipodal quaternion shortest path");
    check(near(KeyframeTrack({{3,end}}).sample(0).position,end.position),"Single key");
    rejects([&] { KeyframeTrack invalid({}); });
    rejects([&] { KeyframeTrack invalid({{0,start},{0,end}}); });
    rejects([&] { KeyframeTrack invalid({{2,start},{1,end}}); });
    Pose bad=start; bad.orientation={0,0,0,0}; rejects([&] { KeyframeTrack invalid({{0,bad}}); });
    rejects([&] { AnimationPlayer invalid(0); });
    for (double invalid : {-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        rejects([&] { once.update(invalid); }); rejects([&] { once.setSpeed(invalid); });
        rejects([&] { AnimationPlayer player(invalid); }); rejects([&] { spin.sample(invalid); });
        rejects([&] { sampleProcedural([](double) { return Pose{}; },invalid); });
        rejects([&] { KeyframeTrack invalidTrack({{invalid,start}}); });
        if (invalid!=-1) rejects([&] { track.sample(invalid); });
    }
    a.setSpeed(2); rejects([&] { a.update(std::numeric_limits<double>::max()); });
    spin.axis={0,0,0}; rejects([&] { spin.sample(0); });
    SimpleAnimation invalidMotion; invalidMotion.motion=static_cast<SimpleMotion>(99);
    rejects([&] { invalidMotion.sample(.25); });
    rejects([] { sampleProcedural({},0); });
    rejects([&] { sampleProcedural([&](double) { return bad; },0); });
    ProceduralAnimation wave=[](double t) { Pose p; p.position.y=float(2*std::sin(t*3)); return p; };
    check(near(sampleProcedural(wave,0.5).matrix(),sampleProcedural(wave,0.5).matrix()),"Deterministic procedural");
    check(!near(sampleProcedural(wave,0).position,sampleProcedural(wave,0.5).position),"Procedural varies with time");
    std::cout << "30/144 FPS, simple rotation/translation/oscillation, lerp/SLERP/normalization/antipodes, playback states and invalid times PASS\n";
}
void demoIntegration(const char* assetsPath) {
    AssetManager assets(assetsPath); const auto imported=assets.loadModel(demo::importedModel);
    const auto originalNodes=imported->nodes;
    std::weak_ptr<Model> weakCubes, weakOutline;
    {
        InteractionAnimationDemo a(*imported), b(*imported);
        weakCubes=a.cubes; weakOutline=a.outline;
        check(a.interaction.objects().size()==8,"Demo examples and import");
        check(near(a.interaction.world("imported"),demo::importedPlacement.matrix()),"Imported base matches historical placement");
        for (int i=0;i<30;++i) a.update(1.0/30);
        for (int i=0;i<144;++i) b.update(1.0/144);
        for (const auto& o:a.interaction.objects()) check(near(a.interaction.world(o.id),b.interaction.world(o.id)),"All demo animation FPS independence");
        const auto world=a.interaction.world("animated-child");
        a.update(0.5); check(!near(world,a.interaction.world("animated-child")),"Animated child changes world");
        a.interaction.select("imported"); a.interaction.manipulate({1,0,0},glm::quat(1,0,0,0),1.1f); a.synchronize();
        for (std::size_t i=0;i<originalNodes.size();++i) check(near(originalNodes[i].local,imported->nodes[i].local),"Original glTF matrices preserved");
        a.togglePause(); auto paused=a.interaction.world("keyframes"); a.update(2);
        check(near(paused,a.interaction.world("keyframes")),"Demo pause");
        a.restart(); check(a.time()==0,"Demo restart");
        check(near(a.interaction.object("imported").manipulation.position,{1,0,0}),"Restart preserves user edits");
    }
    check(weakCubes.expired() && weakOutline.expired(),"Demo resources released");
    rejects([] { InteractionAnimationDemo invalid(Model{}); });
    std::cout << "Demo CPU: all animation families, GLB placement/original matrices, hierarchy, deterministic FPS, cleanup and partial initialization failure PASS\n";
}
}
int main(int argc,char** argv) {
    try {
        if (argc!=2) throw std::runtime_error("Need assets root");
        picking(); hierarchy(); animation(); demoIntegration(argv[1]); return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
