#include "BattleFormationTask.h"

#include "AsstRanges.hpp"

#include "Controller.h"
#include "OcrWithFlagTemplImageAnalyzer.h"
#include "ProcessTask.h"
#include "Resource.h"

#include "AsstRanges.hpp"

void asst::BattleFormationTask::set_stage_name(std::string name)
{
    m_stage_name = std::move(name);
}

bool asst::BattleFormationTask::_run()
{
    const auto& copilot = Resrc.copilot();
    if (!copilot.contains_actions(m_stage_name)) {
        return false;
    }

    m_groups = copilot.get_actions(m_stage_name).groups;
    if (m_groups.empty()) {
        return true;
    }

    if (!enter_selection_page()) {
        return false;
    }

    json::value info = basic_info_with_what("BattleFormation");
    auto& details = info["details"];
    auto& formation = details["formation"];
    for (const auto& name : m_groups | views::keys) {
        formation.array_emplace(name);
    }
    callback(AsstMsg::SubTaskExtraInfo, info);

    // TODO: 需要加一个滑到头了的检测
    while (!need_exit()) {
        select_opers_in_cur_page();
        if (m_groups.empty()) {
            break;
        }
        swipe_page();
    }
    confirm_selection();
    return true;
}

bool asst::BattleFormationTask::enter_selection_page()
{
    return ProcessTask(*this, { "BattleQuickFormation" }).run();
}

bool asst::BattleFormationTask::select_opers_in_cur_page()
{
    auto formation_task_ptr = Task.get("BattleQuickFormationOCR");
    auto image = m_ctrler->get_image();
    auto& ocr_replace = Task.get<OcrTaskInfo>("CharsNameOcrReplace")->replace_map;

    OcrWithFlagTemplImageAnalyzer name_analyzer(image);
    name_analyzer.set_task_info("BattleQuickFormation-OperNameFlag", "BattleQuickFormationOCR");
    name_analyzer.set_replace(ocr_replace);
    name_analyzer.analyze();

    OcrWithFlagTemplImageAnalyzer special_focus_analyzer(image);
    special_focus_analyzer.set_task_info("BattleQuickFormation-OperNameFlag-SpecialFocus", "BattleQuickFormationOCR");
    special_focus_analyzer.set_replace(ocr_replace);
    special_focus_analyzer.analyze();

    auto opers_result = name_analyzer.get_result();
    const auto& special_focus_res = special_focus_analyzer.get_result();
    opers_result.insert(opers_result.end(), special_focus_res.cbegin(), special_focus_res.cend());

    if (opers_result.empty()) {
        return false;
    }

    // 按位置排个序
    ranges::sort(opers_result, [](const TextRect& lhs, const TextRect& rhs) -> bool {
        if (std::abs(lhs.rect.y - rhs.rect.y) < 5) { // y差距较小则理解为是同一排的，按x排序
            return lhs.rect.x < rhs.rect.x;
        }
        else {
            return lhs.rect.y < rhs.rect.y;
        }
    });

    static const std::array<Rect, 3> SkillRectArray = { Task.get("BattleQuickFormationSkill1")->specific_rect,
                                                        Task.get("BattleQuickFormationSkill2")->specific_rect,
                                                        Task.get("BattleQuickFormationSkill3")->specific_rect };

    int skill = 1;
    for (const auto& res : opers_result) {
        const std::string& name = res.text;
        auto iter = m_groups.begin();
        bool found = false;
        for (; iter != m_groups.end(); ++iter) {
            const auto& [_, opers_vec] = *iter;
            for (const auto& oper : opers_vec) {
                if (oper.name == name) {
                    found = true;
                    skill = oper.skill;
                    break;
                }
            }
            if (found) {
                break;
            }
        }

        if (iter == m_groups.end()) {
            continue;
        }
        m_ctrler->click(res.rect);
        sleep(formation_task_ptr->rear_delay);
        if (1 <= skill && skill <= 3) {
            m_ctrler->click(SkillRectArray.at(skill - 1ULL));
        }
        m_groups.erase(iter);

        json::value info = basic_info_with_what("BattleFormationSelected");
        auto& details = info["details"];
        details["selected"] = name;
        callback(AsstMsg::SubTaskExtraInfo, info);
    }

    return true;
}

void asst::BattleFormationTask::swipe_page()
{
    static Rect begin_rect = Task.get("InfrastOperListSwipeBegin")->specific_rect;
    static Rect end_rect = Task.get("InfrastOperListSwipeEnd")->specific_rect;
    static int duration = Task.get("InfrastOperListSwipeBegin")->pre_delay;

    m_ctrler->swipe(begin_rect, end_rect, duration, true, Task.get("InfrastOperListSwipeBegin")->rear_delay, true);
}

bool asst::BattleFormationTask::confirm_selection()
{
    return ProcessTask(*this, { "BattleQuickFormationConfirm" }).run();
}
