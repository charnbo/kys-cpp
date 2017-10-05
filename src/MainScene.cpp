#include "MainScene.h"
#include <time.h>
#include "File.h"
#include "TextureManager.h"
#include "SubScene.h"
#include "Save.h"
#include "UI.h"

MainScene MainScene::main_map_;

MainScene::MainScene()
{
    full_window_ = 1;

    if (!data_readed_)
    {
        earth_layer_.resize(COORD_COUNT);
        surface_layer_.resize(COORD_COUNT);
        building_layer_.resize(COORD_COUNT);
        build_x_layer_.resize(COORD_COUNT);
        build_y_layer_.resize(COORD_COUNT);

        int length = COORD_COUNT * COORD_COUNT * sizeof(SAVE_INT);

        File::readFile("../game/resource/earth.002", &earth_layer_.data(0), length);
        File::readFile("../game/resource/surface.002", &surface_layer_.data(0), length);
        File::readFile("../game/resource/building.002", &building_layer_.data(0), length);
        File::readFile("../game/resource/buildx.002", &build_x_layer_.data(0), length);
        File::readFile("../game/resource/buildy.002", &build_y_layer_.data(0), length);

        divide2(earth_layer_);
        divide2(surface_layer_);
        divide2(building_layer_);
    }
    data_readed_ = true;

    //100个云
    for (int i = 0; i < 100; i++)
    {
        auto c = new Cloud();
        cloud_vector_.push_back(c);
        c->initRand();
    }
    //getEntrance();
}

MainScene::~MainScene()
{
    for (int i = 0; i < cloud_vector_.size(); i++)
    {
        delete cloud_vector_[i];
    }
}

void MainScene::divide2(MapSquare& m)
{
    for (int i = 0; i < m.squareSize(); i++)
    {
        m.data(i) /= 2;
    }
}

void MainScene::draw()
{
    Engine::getInstance()->setRenderAssistTexture();
    //LOG("main\n");
    int k = 0;
    auto t0 = Engine::getInstance()->getTicks();
    struct DrawInfo { int i; Point p; };
    std::map<int, DrawInfo> map;
    //TextureManager::getInstance()->renderTexture("mmap", 0, 0, 0);
#ifdef _DEBUG
    Engine::getInstance()->fillColor({ 0, 0, 0, 255 }, 0, 0, -1, -1);
#endif
    //下面的15是下方较高贴图的余量，其余场景同
    for (int sum = -view_sum_region_; sum <= view_sum_region_ + 15; sum++)
    {
        for (int i = -view_width_region_; i <= view_width_region_; i++)
        {
            int i1 = man_x_ + i + (sum / 2);
            int i2 = man_y_ - i + (sum - sum / 2);
            auto p = getPositionOnScreen(i1, i2, man_x_, man_y_);
            p.x += x_;
            p.y += y_;
            //auto p = getMapPoint(i1, i2, *_Mx, *_My);
            if (!isOutLine(i1, i2))
            {
                //共分3层，地面，表面，建筑，主角包括在建筑中
#ifndef _DEBUG
                //调试模式下不画出地面，图的数量太多占用CPU很大
                if (earth_layer_.data(i1, i2) > 0)
                {
                    TextureManager::getInstance()->renderTexture("mmap", earth_layer_.data(i1, i2), p.x, p.y);
                }
#endif
                if (surface_layer_.data(i1, i2) > 0)
                {
                    TextureManager::getInstance()->renderTexture("mmap", surface_layer_.data(i1, i2), p.x, p.y);
                }
                if (building_layer_.data(i1, i2) > 0)
                {
                    auto t = building_layer_.data(i1, i2);
                    //根据图片的宽度计算图的中点, 为避免出现小数, 实际是中点坐标的2倍
                    //次要排序依据是y坐标
                    //直接设置z轴
                    auto tex = TextureManager::getInstance()->loadTexture("mmap", t);
                    auto w = tex->w;
                    auto h = tex->h;
                    auto dy = tex->dy;
                    int c = ((i1 + i2) - (w + 35) / 36 - (dy - h + 1) / 9) * 1024 + i1;
                    map[2 * c + 1] = { t, p };
                }
                if (i1 == man_x_ && i2 == man_y_)
                {
                    if (isWater(man_x_, man_y_))
                    {
                        man_pic_ = SHIP_PIC_0 + Scene::towards_ * SHIP_PIC_COUNT + step_;
                    }
                    else
                    {
                        man_pic_ = MAN_PIC_0 + Scene::towards_ * MAN_PIC_COUNT + step_;  //每个方向的第一张是静止图
                        if (rest_time_ >= BEGIN_REST_TIME)
                        {
                            man_pic_ = REST_PIC_0 + Scene::towards_ * REST_PIC_COUNT + (rest_time_ - BEGIN_REST_TIME) / REST_INTERVAL % REST_PIC_COUNT;
                        }
                    }
                    int c = 1024 * (i1 + i2) + i1;
                    map[2 * c] = { man_pic_, p };
                }
            }
            k++;
        }
    }
    for (auto i = map.begin(); i != map.end(); i++)
    {
        TextureManager::getInstance()->renderTexture("mmap", i->second.i, i->second.p.x, i->second.p.y);
    }
    auto t1 = Engine::getInstance()->getTicks();
    //云的贴图
    for (auto& c : cloud_vector_)
    {
        //c->draw();
        c->setPositionOnScreen(man_x_, man_y_, screen_center_x_, screen_center_y_);
    }
    //log("%d\n", t1 - t0);
    Engine::getInstance()->renderAssistTextureToWindow();
}

//计时器，负责画图以及一些其他问题
void MainScene::dealEvent(BP_Event& e)
{
    int x = man_x_, y = man_y_;
    //功能键
    if (e.type == BP_KEYUP && e.key.keysym.sym == BPK_ESCAPE
        || e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_RIGHT)
    {
        UI::getInstance()->run();
        clearEvent(e);
    }

    //键盘走路部分，检测4个方向键
    int pressed = 0;
    for (auto i = int(BPK_RIGHT); i <= int(BPK_UP); i++)
    {
        if (i != pre_pressed_ && Engine::getInstance()->checkKeyPress(i))
        {
            pressed = i;
        }
    }
    if (pressed == 0 && Engine::getInstance()->checkKeyPress(pre_pressed_))
    {
        pressed = pre_pressed_;
    }
    pre_pressed_ = pressed;

    if (pressed)
    {
        auto tw = getTowardsFromKey(pressed);
        if (tw != Towards_None) { towards_ = tw; }
        getTowardsPosition(man_x_, man_y_, towards_, &x, &y);
        tryWalk(x, y);
        if (total_step_ < 1)
        {
            Engine::getInstance()->delay(100);
        }
        total_step_++;
    }
    else
    {
        total_step_ = 0;
    }

    if (pressed && checkEntrance(x, y))
    {
        clearEvent(e);
        total_step_ = 0;
    }

    rest_time_++;

    //鼠标寻路，未完成
    if (e.type == BP_MOUSEBUTTONUP && e.button.button == BP_BUTTON_LEFT)
    {
        //getMousePosition(e.button.x, e.button.y);
        //stopFindWay();
        //if (canWalk(mouse_x_, mouse_y_) && !isOutScreen(mouse_x_, mouse_y_))
        //{
        //    FindWay(man_x_, man_y_, mouse_x_, mouse_y_);
        //}
    }
}

void MainScene::onEntrance()
{
    calViewRegion();
    man_x_ = Save::getInstance()->MainMapX;
    man_y_ = Save::getInstance()->MainMapY;
}

void MainScene::onExit()
{
}

void MainScene::tryWalk(int x, int y)
{
    if (canWalk(x, y))
    {
        man_x_ = x;
        man_y_ = y;
    }
    step_++;
    if (isWater(man_x_, man_y_))
    {
        step_ = step_ % SHIP_PIC_COUNT;
    }
    else
    {
        if (step_ >= MAN_PIC_COUNT)
        {
            step_ = 1;
        }
    }
    rest_time_ = 0;
}

bool MainScene::isBuilding(int x, int y)
{

    if (building_layer_.data(build_x_layer_.data(x, y), build_y_layer_.data(x, y)) > 0)
    {
        return  true;
    }
    else
    {
        return false;
    }
}

bool MainScene::isWater(int x, int y)
{
    auto pic = earth_layer_.data(x, y);
    if (pic == 419 || pic >= 306 && pic <= 335)
    {
        return true;
    }
    else if (pic >= 179 && pic <= 181
        || pic >= 253 && pic <= 335
        || pic >= 508 && pic <= 511)
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool MainScene::isOutLine(int x, int y)
{
    return (x < 0 || x > COORD_COUNT || y < 0 || y > COORD_COUNT);
}

bool MainScene::canWalk(int x, int y)
{
    if (isBuilding(x, y) || isOutLine(x, y)/*|| checkIsWater(x, y)*/)
    {
        return false;
    }
    else
    {
        return true;
    }
}

bool MainScene::checkEntrance(int x, int y)
{
    for (int i = 0; i < Save::getInstance()->submap_infos_.size(); i++)
    {
        auto s = Save::getInstance()->getSubMapInfo(i);
        if (x == s->MainEntranceX1 && y == s->MainEntranceY1 || x == s->MainEntranceX2 && y == s->MainEntranceY2)
        {
            bool can_enter = false;
            if (s->EntranceCondition == 0)
            {
                can_enter = true;
            }
            else if (s->EntranceCondition == 2)
            {
                //注意进入条件2的设定
                for (auto r : Save::getInstance()->Team)
                {
                    if (Save::getInstance()->getRole(r)->Speed >= 70)
                    {
                        can_enter = true;
                        break;
                    }
                }
            }
            if (can_enter)
            {
                //这里看起来要主动多画一帧，待修
                drawAll();
                checkEventAndPresent();
                auto sub_map = new SubScene(i);
                sub_map->run();
                return true;
            }
        }
    }
    return false;
}

void MainScene::setEntrance()
{
}

bool MainScene::isOutScreen(int x, int y)
{
    return (abs(man_x_ - x) >= 2 * view_width_region_ || abs(man_y_ - y) >= view_sum_region_);
}

