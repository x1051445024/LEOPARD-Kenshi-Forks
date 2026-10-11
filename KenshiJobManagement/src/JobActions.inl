// SPDX-License-Identifier: GPL-3.0-only
    bool GetWidgetIndex(MyGUI::Widget* widget, const char* key, int* valueOut)
    {
        if (widget == NULL || valueOut == NULL || !widget->isUserString(key))
        {
            return false;
        }
        *valueOut = std::atoi(widget->getUserString(key).c_str());
        return true;
    }

    struct PendingGeneralMemberDrop
    {
        bool pending;
        HandleIdentity sourceSquad;
        HandleIdentity sourceMember;
        HandleIdentity destinationSquad;
        HandleIdentity destinationMember;
        std::vector<JobRowSnapshot> sourceSequence;
        std::vector<JobRowSnapshot> destinationSequence;
        int sourceSlot;
        int destinationGap;

        PendingGeneralMemberDrop() :
            pending(false), sourceSlot(-1), destinationGap(-1)
        {
        }
    };

    PendingGeneralMemberDrop g_pendingGeneralMemberDrop;
    bool g_pendingSquadPriority = false;
    HandleIdentity g_pendingSquadPriorityIdentity;

    void QueueGeneralMemberTransfer(
        int sourceVisibleIndex,
        int sourceSlot,
        int destinationVisibleIndex,
        int destinationGap)
    {
        if (g_pendingGeneralMemberDrop.pending || g_pendingSquadPriority ||
            HasPendingJobBatchUiAction() ||
            g_pendingAction.type != ACTION_NONE ||
            sourceVisibleIndex < 0 || destinationVisibleIndex < 0 ||
            sourceVisibleIndex == destinationVisibleIndex)
        {
            return;
        }
        const MemberSnapshot* source = GetVisibleMember(sourceVisibleIndex);
        const MemberSnapshot* destination =
            GetVisibleMember(destinationVisibleIndex);
        if (source == NULL || destination == NULL ||
            !source->loaded || !source->queueAvailable || source->truncated ||
            !destination->loaded || !destination->queueAvailable ||
            destination->truncated ||
            sourceSlot < 0 ||
            sourceSlot >= static_cast<int>(source->jobs.size()) ||
            destinationGap < 0 ||
            destinationGap > static_cast<int>(destination->jobs.size()))
        {
            SetStatus(Localize(
                "status.queue.readonly",
                "The source or destination queue is read-only. No jobs were changed."));
            return;
        }

        PendingGeneralMemberDrop drop;
        drop.pending = true;
        drop.sourceSquad =
            g_visibleMemberBindings[sourceVisibleIndex].squad;
        drop.sourceMember = source->identity;
        drop.destinationSquad =
            g_visibleMemberBindings[destinationVisibleIndex].squad;
        drop.destinationMember = destination->identity;
        drop.sourceSequence = source->jobs;
        drop.destinationSequence = destination->jobs;
        drop.sourceSlot = sourceSlot;
        drop.destinationGap = destinationGap;
        // MyGUI callbacks publish value-only intent. Live queue reads and all
        // engine mutation happen later from TickMultiSquadActions.
        g_pendingGeneralMemberDrop = drop;
    }

    bool GeneralQueueMatchesPresentation(
        const std::vector<JobRowSnapshot>& presentation,
        const GeneralJobQueueValue& structural)
    {
        if (presentation.size() != structural.rows.size())
        {
            return false;
        }
        for (size_t index = 0; index < presentation.size(); ++index)
        {
            if (presentation[index].taskType !=
                    structural.rows[index].taskType ||
                presentation[index].hasTarget !=
                    structural.rows[index].subjectIdentity.valid ||
                !SameHandleIdentity(
                    presentation[index].target,
                    structural.rows[index].subjectIdentity))
            {
                return false;
            }
        }
        return true;
    }

    int FindPendingSelectedJobMatch(
        const std::vector<SelectedJob>& selected,
        const std::vector<bool>& matched,
        const HandleIdentity& member,
        const JobRowSnapshot& job,
        int slot)
    {
        for (size_t index = 0; index < selected.size(); ++index)
        {
            if (matched[index] ||
                !SameHandleIdentity(selected[index].member, member) ||
                !SameJob(selected[index].job, job) ||
                (job.taskToken == 0 && selected[index].lastSlot != slot))
            {
                continue;
            }
            return static_cast<int>(index);
        }
        return -1;
    }

    bool BuildJobBatchSelectionsFromUiValues(
        const std::vector<SelectedJob>& selected,
        std::vector<JobBatchSelectionValue>* selectionsOut)
    {
        if (selectionsOut == NULL || selected.empty() ||
            !RefreshAllActiveSquadsSnapshot())
        {
            return false;
        }
        selectionsOut->clear();
        std::vector<bool> matched(selected.size(), false);
        size_t matchedCount = 0;

        // Traverse the fresh all-squad snapshot in vanilla board order, then
        // each member queue in priority order. Clipboard and batch moves must
        // not depend on click-selection order.
        for (size_t squadIndex = 0;
             squadIndex < g_allSquads.squads.size(); ++squadIndex)
        {
            const SquadSnapshot& squad = g_allSquads.squads[squadIndex];
            for (size_t memberIndex = 0;
                 memberIndex < squad.members.size(); ++memberIndex)
            {
                const MemberSnapshot& member = squad.members[memberIndex];
                std::vector<int> slots;
                std::vector<int> selectedIndices;
                for (size_t slot = 0; slot < member.jobs.size(); ++slot)
                {
                    const int selectedIndex = FindPendingSelectedJobMatch(
                        selected, matched, member.identity,
                        member.jobs[slot], static_cast<int>(slot));
                    if (selectedIndex >= 0)
                    {
                        slots.push_back(static_cast<int>(slot));
                        selectedIndices.push_back(selectedIndex);
                    }
                }
                if (slots.empty())
                {
                    continue;
                }
                if (!member.loaded || !member.queueAvailable ||
                    member.truncated)
                {
                    return false;
                }
                GeneralJobQueueValue structural;
                if (!TryCaptureGeneralJobQueue(
                        member.identity, &structural) ||
                    !GeneralQueueMatchesPresentation(
                        member.jobs, structural))
                {
                    return false;
                }
                for (size_t index = 0; index < slots.size(); ++index)
                {
                    JobBatchSelectionValue selection;
                    selection.sourceBefore = structural;
                    selection.sourceSlot = slots[index];
                    selectionsOut->push_back(selection);
                    matched[selectedIndices[index]] = true;
                    ++matchedCount;
                }
            }
        }
        return matchedCount == selected.size() && !selectionsOut->empty();
    }

    void CopySelectedJobsToBatchClipboard()
    {
        if (!JobCardSelectionMode())
        {
            ShowToast(Localize(
                "status.clipboard.select_jobs",
                "Select one or more jobs before copying."));
            return;
        }
        const std::vector<SelectedJob> selected = g_selectedJobs;
        std::vector<JobBatchSelectionValue> selections;
        JobBatchActionOutcome outcome;
        if (!BuildJobBatchSelectionsFromUiValues(selected, &selections) ||
            !CaptureJobBatchClipboard(selections, &outcome))
        {
            ClearJobBatchClipboard();
            RefreshSquadView(true);
            const std::string copied = Localize(
                "status.clipboard.jobs_copied", "Jobs copied.");
            const std::string message = BuildJobBatchActionMessage(
                outcome, copied.c_str()) + Localize(
                    "status.clipboard.cleared_suffix",
                    " Clipboard cleared.");
            SetStatus(message);
            ShowToast(message);
            return;
        }
        RefreshSquadView(true);
        const std::string message = LocalizeFormat(
            "status.clipboard.copy_success",
            "Copied jobs: {count}. Select recipients, then press Ctrl+V.",
            "{count}", IntegerString(outcome.consideredBundles));
        SetStatus(message);
        ShowToast(message);
    }

    void QueueJobBatchPaste()
    {
        if (HasPendingJobBatchUiAction())
        {
            return;
        }
        if (!HasJobBatchClipboard())
        {
            ShowToast(Localize(
                "status.clipboard.copy_before_paste",
                "Copy one or more jobs before pasting."));
            return;
        }
        if (!RecipientSelectionMode())
        {
            ShowToast(Localize(
                "status.clipboard.select_recipient",
                "Select at least one recipient before pasting."));
            return;
        }
        const std::vector<HandleIdentity> recipients =
            GetSelectedRecipientIdentities();
        if (recipients.empty())
        {
            ShowToast(Localize(
                "status.clipboard.select_recipient",
                "Select at least one recipient before pasting."));
            return;
        }
        g_pendingJobBatchUiAction = PendingJobBatchUiAction();
        g_pendingJobBatchUiAction.type = JOB_BATCH_UI_PASTE;
        g_pendingJobBatchUiAction.recipients = recipients;
    }

    void TickJobBatchHotkeys()
    {
        const bool controlDown =
            (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const bool copyDown = controlDown &&
            (GetAsyncKeyState('C') & 0x8000) != 0;
        const bool pasteDown = controlDown &&
            (GetAsyncKeyState('V') & 0x8000) != 0;
        const bool available = g_window != NULL && !g_stationTabActive &&
            g_modal.kind == MODAL_NONE && !IsStationDetailOpen() &&
            !g_drag.armed && g_pendingAction.type == ACTION_NONE &&
            !g_pendingGeneralMemberDrop.pending &&
            !g_pendingSquadPriority && !HasPendingJobBatchUiAction();
        if (available && copyDown && !g_copyHotkeyWasDown)
        {
            CopySelectedJobsToBatchClipboard();
        }
        if (available && pasteDown && !g_pasteHotkeyWasDown)
        {
            QueueJobBatchPaste();
        }
        g_copyHotkeyWasDown = copyDown;
        g_pasteHotkeyWasDown = pasteDown;
    }

    void ProcessPendingJobBatchUiAction()
    {
        if (!HasPendingJobBatchUiAction())
        {
            return;
        }
        const PendingJobBatchUiAction action = g_pendingJobBatchUiAction;
        g_pendingJobBatchUiAction = PendingJobBatchUiAction();

        JobBatchActionOutcome outcome;
        std::string completed = Localize(
            "status.batch.action_completed", "Job action completed.");
        if (action.type == JOB_BATCH_UI_PASTE)
        {
            PasteJobBatchClipboard(action.recipients, &outcome);
            completed = Localize(
                "status.batch.jobs_pasted", "Jobs pasted.");
        }
        else if (action.type == JOB_BATCH_UI_MOVE)
        {
            std::vector<JobBatchSelectionValue> selections;
            if (!BuildJobBatchSelectionsFromUiValues(
                    action.selectedJobs, &selections))
            {
                outcome.code = JOB_BATCH_CAPTURE_FAILED;
                outcome.interrupted = true;
            }
            else
            {
                MoveSelectedJobBatch(
                    selections, action.destination, &outcome);
            }
            completed = Localize(
                "status.batch.jobs_moved", "Jobs moved.");
        }
        else if (action.type == JOB_BATCH_UI_ADD_HEALING)
        {
            ApplyJobBatchHealing(action.recipients, true, &outcome);
            completed = Localize(
                "status.batch.healing_jobs_updated",
                "Healing jobs updated.");
        }
        else if (action.type == JOB_BATCH_UI_PRIORITIZE_HEALING)
        {
            ApplyJobBatchHealing(action.recipients, false, &outcome);
            completed = Localize(
                "status.batch.healing_priorities_updated",
                "Healing priorities updated.");
        }
        else if (action.type == JOB_BATCH_UI_REMOVE_INVALID)
        {
            RemoveInvalidJobBatch(action.recipients, &outcome);
            completed = Localize(
                "status.batch.invalid_cleanup_completed",
                "Invalid-job cleanup completed.");
        }
        else
        {
            outcome.code = JOB_BATCH_INVALID_REQUEST;
        }

        if (outcome.destinationChanged || outcome.sourceChanged ||
            outcome.addedHealingJobs != 0 ||
            outcome.movedPriorityRows != 0)
        {
            TryNotifyVanillaSelectionUI();
        }
        RefreshSquadView(true);
        const std::string message =
            action.type == JOB_BATCH_UI_REMOVE_INVALID &&
            outcome.code == JOB_BATCH_NOTHING_TO_DO ?
                Localize(
                    "status.batch.no_removable_invalid",
                    "No removable invalid jobs were found.") :
                BuildJobBatchActionMessage(outcome, completed.c_str());
        SetStatus(message);
        ShowToast(message);
    }

    std::string GeneralTransferStatus(GeneralJobTransferCode code)
    {
        switch (code)
        {
        case GENERAL_TRANSFER_SUCCESS:
            return Localize(
                "status.transfer.success",
                "Permanent job moved to the destination member.");
        case GENERAL_TRANSFER_DISABLED:
            return Localize(
                "status.transfer.disabled",
                "General job transfer is disabled in this test build.");
        case GENERAL_TRANSFER_DUPLICATE:
            return Localize(
                "status.transfer.duplicate",
                "The destination already has the same job. No jobs were changed.");
        case GENERAL_TRANSFER_DESTINATION_FULL:
            return Localize(
                "status.transfer.destination_full",
                "The destination queue is at the safety limit. No jobs were changed.");
        case GENERAL_TRANSFER_SOURCE_CHANGED:
        case GENERAL_TRANSFER_DESTINATION_CHANGED:
            return Localize(
                "status.transfer.queue_changed",
                "A queue changed before the transfer. Review both members and try again.");
        case GENERAL_TRANSFER_ADD_UNEXPECTED_REVIEW:
        case GENERAL_TRANSFER_INSERT_FAILED_REVIEW:
        case GENERAL_TRANSFER_SOURCE_CHANGED_DUPLICATE_REMAINS:
        case GENERAL_TRANSFER_REMOVE_FAILED_DUPLICATE_REMAINS:
        case GENERAL_TRANSFER_REMOVE_UNEXPECTED_REVIEW:
            return Localize(
                "status.transfer.interrupted",
                "The transfer was interrupted. Review both queues before another change.");
        default:
            return Localize(
                "status.transfer.rejected",
                "Kenshi rejected the job transfer. No further jobs were changed.");
        }
    }

    void TickMultiSquadActions()
    {
        if (g_squadGroupRebuildRequested)
        {
            g_squadGroupRebuildRequested = false;
            RebuildSquadWidgets();
        }
        if (HasPendingJobBatchUiAction())
        {
            ProcessPendingJobBatchUiAction();
            return;
        }
        if (g_pendingGeneralMemberDrop.pending)
        {
            const PendingGeneralMemberDrop drop =
                g_pendingGeneralMemberDrop;
            g_pendingGeneralMemberDrop = PendingGeneralMemberDrop();
            int sourceSquadIndex = -1;
            int sourceMemberIndex = -1;
            int destinationSquadIndex = -1;
            int destinationMemberIndex = -1;
            if (!RefreshAllActiveSquadsSnapshot() ||
                !TryFindAllSquadMemberIndices(
                    drop.sourceSquad, drop.sourceMember,
                    &sourceSquadIndex, &sourceMemberIndex) ||
                !TryFindAllSquadMemberIndices(
                    drop.destinationSquad, drop.destinationMember,
                    &destinationSquadIndex, &destinationMemberIndex))
            {
                RefreshSquadView(true);
                SetStatus(Localize(
                    "status.transfer.roster_member_left",
                    "The source or destination left the active player roster. No jobs were changed."));
                return;
            }

            const MemberSnapshot* source =
                FindDisplayedMember(drop.sourceMember);
            const MemberSnapshot* destination =
                FindDisplayedMember(drop.destinationMember);
            if (source == NULL || destination == NULL ||
                !source->loaded || !source->queueAvailable ||
                source->truncated || !destination->loaded ||
                !destination->queueAvailable || destination->truncated ||
                !SameQueue(source->jobs, drop.sourceSequence) ||
                !SameQueue(destination->jobs, drop.destinationSequence))
            {
                RefreshSquadView(true);
                SetStatus(Localize(
                    "status.transfer.queue_changed_after_drop",
                    "A source or destination queue changed after the drop. No jobs were changed."));
                return;
            }

            GeneralJobTransferRequest request;
            request.sourceSlot = drop.sourceSlot;
            request.destinationGap = drop.destinationGap;
            if (!TryCaptureGeneralJobQueue(
                    drop.sourceMember, &request.sourceBefore) ||
                !TryCaptureGeneralJobQueue(
                    drop.destinationMember, &request.destinationBefore) ||
                !GeneralQueueMatchesPresentation(
                    drop.sourceSequence, request.sourceBefore) ||
                !GeneralQueueMatchesPresentation(
                    drop.destinationSequence, request.destinationBefore))
            {
                RefreshSquadView(true);
                SetStatus(Localize(
                    "status.transfer.queue_unverified",
                    "A live queue could not be verified for the transfer. No jobs were changed."));
                return;
            }
            GeneralJobTransferOutcome outcome;
            const GeneralJobTransferCode code =
                TryTransferGeneralPermanentJob(request, &outcome);
#ifdef KJM_GENERAL_JOB_TRANSFER_PROBE
            {
                std::ostringstream probeLog;
                probeLog << "[KenshiJobManagement] General transfer probe: code="
                         << static_cast<int>(code)
                         << ", sourceRows=" << request.sourceBefore.rows.size()
                         << ", destinationRows="
                         << request.destinationBefore.rows.size()
                         << ", transferredRows=" << outcome.transferredRows
                         << ", destinationChanged="
                         << (outcome.destinationChanged ? 1 : 0)
                         << ", sourceChanged="
                         << (outcome.sourceChanged ? 1 : 0) << ".";
                DebugLog(probeLog.str().c_str());
            }
#endif
            if (code == GENERAL_TRANSFER_SUCCESS)
            {
                TryNotifyVanillaSelectionUI();
            }
            RefreshSquadView(true);
            const std::string transferStatus = GeneralTransferStatus(code);
            SetStatus(transferStatus);
            ShowToast(transferStatus);
            return;
        }

        if (g_pendingSquadPriority)
        {
            const HandleIdentity squad = g_pendingSquadPriorityIdentity;
            g_pendingSquadPriority = false;
            ResetHandleIdentity(&g_pendingSquadPriorityIdentity);
            SquadPriorityResult result;
            TryApplySquadPriority(squad, &result);
            if (result.changedMembers > 0)
            {
                TryNotifyVanillaSelectionUI();
            }
            RefreshSquadView(true);
            SetStatus(result.message);
        }
    }

    void ResetMultiSquadActions()
    {
        g_pendingJobBatchUiAction = PendingJobBatchUiAction();
        g_pendingGeneralMemberDrop = PendingGeneralMemberDrop();
        g_pendingSquadPriority = false;
        ResetHandleIdentity(&g_pendingSquadPriorityIdentity);
    }

    bool GetCardBinding(MyGUI::Widget* widget, int* memberIndexOut, int* slotOut)
    {
        if (!GetWidgetIndex(widget, "KJM_Member", memberIndexOut) ||
            !GetWidgetIndex(widget, "KJM_Slot", slotOut) ||
            *memberIndexOut < 0 || *slotOut < 0 ||
            *memberIndexOut >= static_cast<int>(g_visibleMemberBindings.size()))
        {
            return false;
        }
        const MemberSnapshot* member = GetVisibleMember(*memberIndexOut);
        return member != NULL &&
            *slotOut < static_cast<int>(member->jobs.size());
    }

    void ClearJobHoverHighlight()
    {
        const int previousGroup = g_hoveredJobHighlightGroup;
        if (previousGroup < 0)
        {
            return;
        }
        g_hoveredJobHighlightGroup = -1;
        ApplyCardHoverHighlights(previousGroup, -1);
    }

    void OnCardMouseSetFocus(MyGUI::Widget* widget, MyGUI::Widget*)
    {
        if (g_modal.kind != MODAL_NONE || g_drag.active)
        {
            ClearJobHoverHighlight();
            return;
        }

        int memberIndex = -1;
        int slot = -1;
        if (!GetCardBinding(widget, &memberIndex, &slot) ||
            memberIndex >= static_cast<int>(g_memberWidgets.size()) ||
            slot >= static_cast<int>(g_memberWidgets[memberIndex].cards.size()))
        {
            ClearJobHoverHighlight();
            return;
        }

        const int group =
            g_memberWidgets[memberIndex].cards[slot].highlightGroup;
        if (group < 0)
        {
            ClearJobHoverHighlight();
            return;
        }
        if (group == g_hoveredJobHighlightGroup)
        {
            return;
        }
        const int previousGroup = g_hoveredJobHighlightGroup;
        g_hoveredJobHighlightGroup = group;
        ApplyCardHoverHighlights(previousGroup, group);
    }

    void OnCardMouseLostFocus(
        MyGUI::Widget*,
        MyGUI::Widget* nextWidget)
    {
        // MyGUI commonly emits lost-focus immediately before set-focus while
        // the pointer crosses adjacent cards. Resolve the incoming card here
        // so one boundary crossing performs one old-to-new delta instead of a
        // full clear followed by a second full show pass.
        if (g_modal.kind == MODAL_NONE && !g_drag.active)
        {
            int memberIndex = -1;
            int slot = -1;
            if (GetCardBinding(nextWidget, &memberIndex, &slot) &&
                memberIndex < static_cast<int>(g_memberWidgets.size()) &&
                slot < static_cast<int>(
                    g_memberWidgets[memberIndex].cards.size()))
            {
                const int nextGroup =
                    g_memberWidgets[memberIndex].cards[slot].highlightGroup;
                if (nextGroup >= 0)
                {
                    if (nextGroup != g_hoveredJobHighlightGroup)
                    {
                        const int previousGroup = g_hoveredJobHighlightGroup;
                        g_hoveredJobHighlightGroup = nextGroup;
                        ApplyCardHoverHighlights(previousGroup, nextGroup);
                    }
                    return;
                }
            }
        }
        ClearJobHoverHighlight();
    }

    int FindSelectedJobIndex(
        const HandleIdentity& member,
        const JobRowSnapshot& job,
        int slot)
    {
        for (size_t index = 0; index < g_selectedJobs.size(); ++index)
        {
            if (SameHandleIdentity(g_selectedJobs[index].member, member) &&
                SameJob(g_selectedJobs[index].job, job) &&
                (job.taskToken != 0 || g_selectedJobs[index].lastSlot == slot))
            {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    void ApplySynchronizedJobSelectionStates()
    {
        SyncRecipientsToSelectedJobs();
        ApplyCardSelectionStates();
    }

    void AddSelectedJob(int memberIndex, int slot)
    {
        if (memberIndex < 0 || slot < 0 ||
            memberIndex >= static_cast<int>(g_visibleMemberBindings.size()))
        {
            return;
        }
        const MemberSnapshot* memberPointer = GetVisibleMember(memberIndex);
        if (memberPointer == NULL ||
            slot >= static_cast<int>(memberPointer->jobs.size()))
        {
            return;
        }
        const MemberSnapshot& member = *memberPointer;
        const JobRowSnapshot& job = member.jobs[slot];
        if (FindSelectedJobIndex(member.identity, job, slot) >= 0)
        {
            return;
        }
        SelectedJob selected;
        selected.member = member.identity;
        selected.job = job;
        selected.lastSlot = slot;
        g_selectedJobs.push_back(selected);
    }

    void SelectOnlyJob(int memberIndex, int slot)
    {
        g_selectedJobs.clear();
        AddSelectedJob(memberIndex, slot);
        const MemberSnapshot* member = memberIndex >= 0 ?
            GetVisibleMember(memberIndex) : NULL;
        if (member != NULL)
        {
            g_selectionAnchorMember = member->identity;
            g_selectionAnchorSlot = slot;
        }
        ApplySynchronizedJobSelectionStates();
    }

    void ApplyCardClickSelection(int memberIndex, int slot)
    {
        const MemberSnapshot* memberPointer = GetVisibleMember(memberIndex);
        if (memberPointer == NULL || slot < 0 ||
            slot >= static_cast<int>(memberPointer->jobs.size()))
        {
            return;
        }
        const MemberSnapshot& member = *memberPointer;
        const JobRowSnapshot& job = member.jobs[slot];
        MyGUI::InputManager& input = MyGUI::InputManager::getInstance();
        const bool control = input.isControlPressed();
        const bool shift = input.isShiftPressed();

        if (shift && SameHandleIdentity(g_selectionAnchorMember, member.identity) &&
            g_selectionAnchorSlot >= 0)
        {
            if (!control)
            {
                g_selectedJobs.clear();
            }
            const int low = std::min(g_selectionAnchorSlot, slot);
            const int high = std::max(g_selectionAnchorSlot, slot);
            for (int rangeSlot = low; rangeSlot <= high; ++rangeSlot)
            {
                if (rangeSlot < static_cast<int>(member.jobs.size()))
                {
                    AddSelectedJob(memberIndex, rangeSlot);
                }
            }
        }
        else if (control)
        {
            const int selectedIndex =
                FindSelectedJobIndex(member.identity, job, slot);
            if (selectedIndex >= 0)
            {
                g_selectedJobs.erase(g_selectedJobs.begin() + selectedIndex);
            }
            else
            {
                AddSelectedJob(memberIndex, slot);
            }
            g_selectionAnchorMember = member.identity;
            g_selectionAnchorSlot = slot;
        }
        else
        {
            SelectOnlyJob(memberIndex, slot);
            return;
        }

        ApplySynchronizedJobSelectionStates();
    }

    void ClearJobSelection()
    {
        if (g_selectedJobs.empty() &&
            !g_selectionAnchorMember.valid &&
            g_selectionAnchorSlot < 0)
        {
            return;
        }
        const bool hadSelectedJobs = !g_selectedJobs.empty();
        g_selectedJobs.clear();
        ResetHandleIdentity(&g_selectionAnchorMember);
        g_selectionAnchorSlot = -1;
        if (hadSelectedJobs)
        {
            SyncRecipientsToSelectedJobs();
        }
        // An already-empty job selection can still have a direct portrait or
        // squad selection. ApplyCardSelectionStates refreshes both layers
        // without changing that direct recipient set.
        ApplyCardSelectionStates();
    }

    void ClearRecipientSelectionForJobInteraction()
    {
        if (g_selectedRecipients.empty())
        {
            return;
        }
        ResetRecipientSelection();
        ApplyRecipientSelectionStates();
        UpdateSquadSelectorSelection();
        UpdateSquadHeading();
    }

    void ClearJobSelectionForRecipientInteraction()
    {
        if (g_selectedJobs.empty() &&
            !g_selectionAnchorMember.valid &&
            g_selectionAnchorSlot < 0)
        {
            return;
        }

        // The selected jobs supplied the initial recipient set. When the
        // player switches to direct recipient selection, keep those owners so
        // Ctrl-click can add or remove a portrait or squad from that set.
        g_selectedJobs.clear();
        ResetHandleIdentity(&g_selectionAnchorMember);
        g_selectionAnchorSlot = -1;
        ApplyCardSelectionStates();
    }

    void OnEmptyPressed(MyGUI::Widget*, int, int, MyGUI::MouseButton button)
    {
        if (button == MyGUI::MouseButton::Left &&
            g_modal.kind == MODAL_NONE &&
            !HasPendingJobBatchUiAction() &&
            g_pendingAction.type == ACTION_NONE &&
            !g_pendingGeneralMemberDrop.pending &&
            !g_pendingSquadPriority)
        {
            // Keep an established Ctrl multi-selection stable when the
            // pointer lands in unused board space. The user can still reduce
            // the set with Ctrl-clicks or replace it by clicking a different
            // job card.
            if (g_selectedJobs.size() > 1)
            {
                // Its derived recipient set remains synchronized with the
                // selected cards.
                return;
            }
            ClearJobSelection();
            ClearRecipientSelectionForJobInteraction();
        }
    }

    bool PointInside(const MyGUI::IntCoord& coord, const MyGUI::IntPoint& point)
    {
        return point.left >= coord.left && point.left < coord.right() &&
            point.top >= coord.top && point.top < coord.bottom();
    }

    void ResetRemoveDropStyle()
    {
        if (g_removeButton != NULL)
        {
            g_removeButton->setColour(MyGUI::Colour::White);
            g_removeButton->setStateSelected(false);
        }
        UpdateRemoveButton();
    }

    void SetDragDestinationOutlineVisible(bool visible)
    {
        if (g_dragDestinationOutlineTop != NULL)
        {
            g_dragDestinationOutlineTop->setVisible(visible);
        }
        if (g_dragDestinationOutlineBottom != NULL)
        {
            g_dragDestinationOutlineBottom->setVisible(visible);
        }
        if (g_dragDestinationOutlineLeft != NULL)
        {
            g_dragDestinationOutlineLeft->setVisible(visible);
        }
        if (g_dragDestinationOutlineRight != NULL)
        {
            g_dragDestinationOutlineRight->setVisible(visible);
        }
    }

    void UpdateDragDestinationOutline(int destinationIndex)
    {
        if (!g_drag.active || g_squadTabRoot == NULL ||
            g_memberViewport == NULL || g_jobViewport == NULL ||
            destinationIndex < 0 ||
            destinationIndex >= static_cast<int>(g_visibleMemberBindings.size()) ||
            g_dragDestinationOutlineTop == NULL ||
            g_dragDestinationOutlineBottom == NULL ||
            g_dragDestinationOutlineLeft == NULL ||
            g_dragDestinationOutlineRight == NULL)
        {
            SetDragDestinationOutlineVisible(false);
            return;
        }

        const MyGUI::IntCoord memberViewport = g_memberViewport->getCoord();
        const MyGUI::IntCoord jobViewport = g_jobViewport->getCoord();
        const int rowTop = memberViewport.top +
            g_visibleMemberBindings[destinationIndex].rowTop -
            g_verticalOffset;
        const int rowBottom = rowTop + ROW_HEIGHT;
        const int clippedTop = std::max(rowTop, memberViewport.top);
        const int clippedBottom = std::min(rowBottom, memberViewport.bottom());
        const int left = memberViewport.left;
        const int right = jobViewport.right();
        const int width = right - left;
        const int visibleHeight = clippedBottom - clippedTop;
        if (width <= 6 || visibleHeight <= 6)
        {
            SetDragDestinationOutlineVisible(false);
            return;
        }

        const int thickness = S(3);
        g_dragDestinationOutlineTop->setCoord(
            left, clippedTop, width, thickness);
        g_dragDestinationOutlineBottom->setCoord(
            left, clippedBottom - thickness, width, thickness);
        g_dragDestinationOutlineLeft->setCoord(
            left, clippedTop, thickness, visibleHeight);
        g_dragDestinationOutlineRight->setCoord(
            right - thickness, clippedTop, thickness, visibleHeight);
        SetDragDestinationOutlineVisible(true);
    }

    void CancelDrag()
    {
        g_drag = DragState();
        g_dragDestinationVisibleIndex = -1;
        g_lastDragTick = 0;
        ClearJobHoverHighlight();
        SetDragDestinationOutlineVisible(false);
        if (g_insertionLine != NULL)
        {
            g_insertionLine->setVisible(false);
        }
        ResetRemoveDropStyle();
        MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
        if (input != NULL)
        {
            try
            {
                input->resetMouseCaptureWidget();
            }
            catch (...)
            {
            }
        }
        // Edge auto-scroll keeps the captured source subtree visible while a
        // drag is armed. Reapply normal viewport culling after capture ends so
        // a canceled drop cannot leave that offscreen subtree active.
        ApplySquadViewportVisibility();
    }

    int FindVisibleMemberRowAtMouse(const MyGUI::IntPoint& mouse)
    {
        if (g_memberViewport == NULL || g_jobViewport == NULL)
        {
            return -1;
        }
        const MyGUI::IntCoord memberView =
            g_memberViewport->getAbsoluteCoord();
        const MyGUI::IntCoord jobView = g_jobViewport->getAbsoluteCoord();
        const bool inMemberView =
            mouse.left >= memberView.left &&
            mouse.left < memberView.right() &&
            mouse.top >= memberView.top &&
            mouse.top < memberView.bottom();
        const bool inJobView =
            mouse.left >= jobView.left &&
            mouse.left < jobView.right() &&
            mouse.top >= jobView.top &&
            mouse.top < jobView.bottom();
        if (!inMemberView && !inJobView)
        {
            return -1;
        }
        for (size_t index = 0;
             index < g_visibleMemberBindings.size(); ++index)
        {
            const int rowTop = jobView.top +
                g_visibleMemberBindings[index].rowTop - g_verticalOffset;
            if (mouse.top >= rowTop && mouse.top < rowTop + ROW_HEIGHT)
            {
                return static_cast<int>(index);
            }
        }
        return -1;
    }

    bool MouseIsInSourceRow(const MyGUI::IntPoint& mouse)
    {
        return FindVisibleMemberRowAtMouse(mouse) == g_drag.memberIndex;
    }

    void UpdateDragInsertion(const MyGUI::IntPoint& mouse)
    {
        if (!g_drag.active ||
            (g_drag.kind != DRAG_REORDER &&
             g_drag.kind != DRAG_MULTI_MOVE) ||
            g_jobViewport == NULL || g_insertionLine == NULL ||
            g_drag.memberIndex < 0 ||
            g_drag.memberIndex >= static_cast<int>(g_visibleMemberBindings.size()))
        {
            if (g_insertionLine != NULL)
            {
                g_insertionLine->setVisible(false);
            }
            SetDragDestinationOutlineVisible(false);
            return;
        }

        const int destinationIndex = FindVisibleMemberRowAtMouse(mouse);
        const MemberSnapshot* destination = destinationIndex >= 0 ?
            GetVisibleMember(destinationIndex) : NULL;
        if (destination == NULL || !destination->queueAvailable)
        {
            g_drag.insertionGap = -1;
            g_dragDestinationVisibleIndex = -1;
            g_insertionLine->setVisible(false);
            SetDragDestinationOutlineVisible(false);
            return;
        }

        const MyGUI::IntCoord view = g_jobViewport->getAbsoluteCoord();
        const int contentX = mouse.left - view.left + g_horizontalOffset;
        const int queueCount =
            static_cast<int>(destination->jobs.size());
        int gap = g_drag.kind == DRAG_MULTI_MOVE ? queueCount :
            (contentX + CARD_STRIDE / 2) / CARD_STRIDE;
        gap = ClampInt(gap, 0, queueCount);
        g_drag.insertionGap = gap;
        g_dragDestinationVisibleIndex = destinationIndex;
        UpdateDragDestinationOutline(destinationIndex);

        MyGUI::Widget* client = g_window != NULL ? g_window->getClientWidget() : NULL;
        if (client == NULL)
        {
            return;
        }
        const MyGUI::IntCoord clientCoord = client->getAbsoluteCoord();
        const int lineX = view.left - clientCoord.left + gap * CARD_STRIDE -
            g_horizontalOffset - 2;
        const int lineY = view.top - clientCoord.top +
            g_visibleMemberBindings[destinationIndex].rowTop - g_verticalOffset;
        g_insertionLine->setCoord(lineX, lineY, S(4), ROW_HEIGHT);
        g_insertionLine->setVisible(true);
    }

    void StartDragVisuals()
    {
        if (g_removeButton != NULL)
        {
            g_removeButton->setColour(MyGUI::Colour(1.0f, 0.34f, 0.30f));
            g_removeButton->setStateSelected(true);
            // Keep the temporary drop target affordance short enough for the
            // button while the normal action remains "Remove Job(s)".
            SetFittedButtonCaption(
                g_removeButton,
                Localize(
                    "ui.action.drop_to_remove", "DROP TO REMOVE"),
                12, 9, 14);
        }
    }

    void OnCardPressed(
        MyGUI::Widget* widget,
        int,
        int,
        MyGUI::MouseButton button)
    {
        if (button != MyGUI::MouseButton::Left ||
            g_modal.kind != MODAL_NONE || HasPendingJobBatchUiAction() ||
            g_pendingAction.type != ACTION_NONE ||
            g_pendingGeneralMemberDrop.pending || g_pendingSquadPriority)
        {
            return;
        }

        int memberIndex = -1;
        int slot = -1;
        if (!GetCardBinding(widget, &memberIndex, &slot))
        {
            return;
        }

        const MemberSnapshot* memberPointer = GetVisibleMember(memberIndex);
        if (memberPointer == NULL || !memberPointer->queueAvailable)
        {
            return;
        }
        const MemberSnapshot& member = *memberPointer;
        const JobRowSnapshot& job = member.jobs[slot];
        // Job-card selection derives its recipient set from the owning
        // members. Direct portrait and squad selection still use their own
        // interaction path and may clear the job cards.
        ResetBottomSquadSelectionTracking();
        MyGUI::InputManager& input = MyGUI::InputManager::getInstance();
        const bool control = input.isControlPressed();
        const bool plain = !control && !input.isShiftPressed();
        const bool alreadySelected =
            FindSelectedJobIndex(member.identity, job, slot) >= 0;

        g_drag = DragState();
        g_drag.armed = true;
        g_drag.pressPoint = input.getMousePosition();
        g_drag.memberIndex = memberIndex;
        g_drag.slot = slot;
        g_drag.startSequence = member.jobs;
        g_drag.deferredPlainClick =
            plain && alreadySelected && g_selectedJobs.size() > 1;
        // A Ctrl-click on a selected card normally toggles it off. Defer that
        // toggle until release so the same press can start a drag without the
        // card disappearing from the multi-selection first.
        g_drag.deferredControlToggle = control && alreadySelected;

        if (!g_drag.deferredPlainClick &&
            !g_drag.deferredControlToggle)
        {
            ApplyCardClickSelection(memberIndex, slot);
        }
    }

    void OnCardDrag(
        MyGUI::Widget*,
        int,
        int,
        MyGUI::MouseButton button)
    {
        if (button != MyGUI::MouseButton::Left || !g_drag.armed)
        {
            return;
        }
        if (g_drag.memberIndex < 0 ||
            g_drag.memberIndex >= static_cast<int>(g_visibleMemberBindings.size()))
        {
            CancelDrag();
            return;
        }
        const MemberSnapshot* sourceMemberPointer =
            GetVisibleMember(g_drag.memberIndex);
        if (sourceMemberPointer == NULL ||
            g_drag.slot < 0 ||
            g_drag.slot >= static_cast<int>(
                sourceMemberPointer->jobs.size()) ||
            !SameQueue(
                sourceMemberPointer->jobs,
                g_drag.startSequence))
        {
            CancelDrag();
            SetStatus(Localize(
                "status.drag.queue_changed",
                "The queue changed during the drag. Review it and try again."));
            return;
        }
        const MyGUI::IntPoint mouse =
            MyGUI::InputManager::getInstance().getMousePosition();
        if (!g_drag.active)
        {
            const int dx = std::abs(mouse.left - g_drag.pressPoint.left);
            const int dy = std::abs(mouse.top - g_drag.pressPoint.top);
            if (dx < DRAG_THRESHOLD && dy < DRAG_THRESHOLD)
            {
                return;
            }
            g_drag.active = true;
            g_drag.deferredPlainClick = false;
            g_drag.deferredControlToggle = false;
            ClearJobHoverHighlight();
            const MemberSnapshot& sourceMember = *sourceMemberPointer;
            if (FindSelectedJobIndex(
                    sourceMember.identity,
                    sourceMember.jobs[g_drag.slot],
                    g_drag.slot) < 0)
            {
                SelectOnlyJob(g_drag.memberIndex, g_drag.slot);
            }
            g_drag.kind = g_selectedJobs.size() > 1
                ? DRAG_MULTI_MOVE
                : DRAG_REORDER;
            StartDragVisuals();
        }
        UpdateDragInsertion(mouse);
    }

    void QueueRemoveSelectedAction()
    {
        if (!JobCardSelectionMode() || g_pendingAction.type != ACTION_NONE)
        {
            return;
        }
        g_pendingAction = PendingAction();
        g_pendingAction.type = ACTION_REMOVE_SELECTED;
    }

    void OnCardReleased(
        MyGUI::Widget*,
        int,
        int,
        MyGUI::MouseButton button)
    {
        if (button != MyGUI::MouseButton::Left || !g_drag.armed)
        {
            return;
        }

        if (g_drag.memberIndex < 0 ||
            g_drag.memberIndex >= static_cast<int>(g_visibleMemberBindings.size()))
        {
            CancelDrag();
            return;
        }
        const MemberSnapshot* sourceMemberPointer =
            GetVisibleMember(g_drag.memberIndex);
        if (sourceMemberPointer == NULL ||
            g_drag.slot < 0 ||
            g_drag.slot >= static_cast<int>(
                sourceMemberPointer->jobs.size()) ||
            !SameQueue(
                sourceMemberPointer->jobs,
                g_drag.startSequence))
        {
            CancelDrag();
            SetStatus(Localize(
                "status.drag.queue_changed",
                "The queue changed during the drag. Review it and try again."));
            return;
        }

        const MyGUI::IntPoint mouse =
            MyGUI::InputManager::getInstance().getMousePosition();
        if (!g_drag.active)
        {
            if (g_drag.deferredControlToggle)
            {
                const MemberSnapshot& sourceMember = *sourceMemberPointer;
                const int selectedIndex = FindSelectedJobIndex(
                    sourceMember.identity,
                    sourceMember.jobs[g_drag.slot],
                    g_drag.slot);
                if (selectedIndex >= 0)
                {
                    g_selectedJobs.erase(
                        g_selectedJobs.begin() + selectedIndex);
                }
                g_selectionAnchorMember = sourceMember.identity;
                g_selectionAnchorSlot = g_drag.slot;
                ApplySynchronizedJobSelectionStates();
            }
            // deferredPlainClick deliberately performs no selection change:
            // a plain click on a member of a multi-set now keeps that set.
            CancelDrag();
            return;
        }

        if (g_removeButton != NULL &&
            PointInside(g_removeButton->getAbsoluteCoord(), mouse))
        {
            QueueRemoveSelectedAction();
            CancelDrag();
            return;
        }

        if (g_drag.kind == DRAG_REORDER && g_drag.insertionGap >= 0 &&
            g_dragDestinationVisibleIndex == g_drag.memberIndex)
        {
            int target = g_drag.insertionGap;
            if (g_drag.insertionGap > g_drag.slot)
            {
                --target;
            }
            const int count = static_cast<int>(g_drag.startSequence.size());
            target = ClampInt(target, 0, std::max(0, count - 1));
            if (target != g_drag.slot && g_pendingAction.type == ACTION_NONE)
            {
                g_pendingAction = PendingAction();
                g_pendingAction.type = ACTION_REORDER;
                g_pendingAction.member =
                    sourceMemberPointer->identity;
                g_pendingAction.job =
                    sourceMemberPointer->jobs[g_drag.slot];
                g_pendingAction.sequence = g_drag.startSequence;
                g_pendingAction.targetSlot = target;
            }
        }
        else if (g_drag.kind == DRAG_REORDER &&
            g_drag.insertionGap >= 0 &&
            g_dragDestinationVisibleIndex >= 0 &&
            g_dragDestinationVisibleIndex != g_drag.memberIndex)
        {
            QueueGeneralMemberTransfer(
                g_drag.memberIndex,
                g_drag.slot,
                g_dragDestinationVisibleIndex,
                g_drag.insertionGap);
        }
        else if (g_drag.kind == DRAG_MULTI_MOVE &&
            g_drag.insertionGap >= 0 &&
            g_dragDestinationVisibleIndex >= 0)
        {
            const MemberSnapshot* destination =
                GetVisibleMember(g_dragDestinationVisibleIndex);
            bool destinationIsSource = false;
            if (destination != NULL)
            {
                for (size_t index = 0;
                     index < g_selectedJobs.size(); ++index)
                {
                    if (SameHandleIdentity(
                            g_selectedJobs[index].member,
                            destination->identity))
                    {
                        destinationIsSource = true;
                        break;
                    }
                }
            }
            if (destination == NULL || destinationIsSource)
            {
                ShowToast(Localize(
                    "status.move.invalid_destination",
                    "Move cancelled: choose a member that does not own any selected source job."));
            }
            else if (!HasPendingJobBatchUiAction() &&
                     g_pendingAction.type == ACTION_NONE &&
                     !g_pendingGeneralMemberDrop.pending)
            {
                g_pendingJobBatchUiAction = PendingJobBatchUiAction();
                g_pendingJobBatchUiAction.type = JOB_BATCH_UI_MOVE;
                g_pendingJobBatchUiAction.selectedJobs = g_selectedJobs;
                g_pendingJobBatchUiAction.destination =
                    destination->identity;
            }
        }

        CancelDrag();
    }

    std::string WrapToolTipCaption(
        const std::string& source,
        size_t maximumCharacters,
        size_t* lineCountOut)
    {
        return WrapUtf8Text(source, maximumCharacters, lineCountOut);
    }

    void OnCardToolTip(MyGUI::Widget* widget, const MyGUI::ToolTipInfo& info)
    {
        MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
        if (gui == NULL || widget == NULL)
        {
            return;
        }
        if (info.type == MyGUI::ToolTipInfo::Hide)
        {
            if (g_tooltip != NULL)
            {
                g_tooltip->setVisible(false);
            }
            return;
        }
        if (g_tooltip == NULL)
        {
            g_tooltip = gui->createWidget<MyGUI::Widget>(
                "Kenshi_FloatingPanelSkin",
                MyGUI::IntCoord(0, 0, S(520), S(104)),
                MyGUI::Align::Default,
                "ToolTip",
                "KJM_ToolTip");
            g_tooltip->setUserString("KJM_Native", "1");   // LEOPARD: game skin, no tint
            TagThemeBackground(g_tooltip);
            g_tooltip->setNeedMouseFocus(false);
            g_tooltipText = g_tooltip->createWidget<MyGUI::TextBox>(
                "Kenshi_TextboxStandardText_Small",
                MyGUI::IntCoord(S(8), S(6), S(504), S(92)),
                MyGUI::Align::Stretch,
                "KJM_ToolTipText");
            g_tooltipText->setNeedMouseFocus(false);
            TagThemeStandardText(g_tooltipText);
        }
        // LEOPARD: an explicit font at the current scale, and the height is
        // measured from the text below - the original 12 + lines * 16 cut the
        // last line off at any scale above 1.
        if (g_tooltipText != NULL)
        {
            SetFontPx(g_tooltipText, S(15));
        }
        const MyGUI::IntSize view =
            MyGUI::RenderManager::getInstance().getViewSize();
        const int tooltipWidth = ClampInt(S(520), S(240), std::max(S(240), view.width - S(20)));
        size_t lineCount = 1;
        if (widget->isUserString("KJM_ToolTip") && g_tooltipText != NULL)
        {
            const size_t maximumCharacters = static_cast<size_t>(std::max(
                28, (tooltipWidth - S(16)) / std::max(1, S(15) / 2)));
            const std::string caption = WrapToolTipCaption(
                widget->getUserString("KJM_ToolTip"),
                maximumCharacters,
                &lineCount);
            g_tooltipText->setCaption(caption);
        }
        (void)lineCount;
        const int textHeight = g_tooltipText != NULL ?
            g_tooltipText->getTextSize().height : S(16);
        const int tooltipHeight = ClampInt(
            textHeight + S(16),
            S(30),
            std::max(S(30), view.height - S(20)));
        g_tooltip->setSize(tooltipWidth, tooltipHeight);
        if (g_tooltipText != NULL)
        {
            g_tooltipText->setCoord(
                S(8), S(6), tooltipWidth - S(16), tooltipHeight - S(12));
        }
        const int x = ClampInt(
            info.point.left + S(16), 0, std::max(0, view.width - tooltipWidth));
        const int y = ClampInt(
            info.point.top + S(20), 0, std::max(0, view.height - tooltipHeight));
        g_tooltip->setPosition(x, y);
        g_tooltip->setVisible(true);
    }

    void OnVerticalScroll(MyGUI::ScrollBar*, size_t position)
    {
        if (g_changingScroll)
        {
            return;
        }
        ClearJobHoverHighlight();
        g_verticalOffset = ClampInt(
            static_cast<int>(position), 0, g_maxVerticalOffset);
        ApplyScrollOffsets();
        StoreCurrentSquadScrollOffsets();
    }

    void OnHorizontalScroll(MyGUI::ScrollBar*, size_t position)
    {
        if (g_changingScroll)
        {
            return;
        }
        ClearJobHoverHighlight();
        g_horizontalOffset = ClampInt(
            static_cast<int>(position), 0, g_maxHorizontalOffset);
        ApplyScrollOffsets();
        StoreCurrentSquadScrollOffsets();
        if (g_drag.active)
        {
            UpdateDragInsertion(MyGUI::InputManager::getInstance().getMousePosition());
        }
    }

    int NormalizeMouseWheelNotches(int relative)
    {
        if (relative == 0)
        {
            return 0;
        }
        if (relative >= WHEEL_DELTA || relative <= -WHEEL_DELTA)
        {
            const int notches = relative / WHEEL_DELTA;
            return notches == 0 ? (relative > 0 ? 1 : -1) : notches;
        }
        return relative > 0 ? 1 : -1;
    }

    void OnMouseWheel(MyGUI::Widget*, int relative)
    {
        if (relative == 0 || g_window == NULL || g_stationTabActive ||
            g_modal.kind != MODAL_NONE || IsStationDetailOpen())
        {
            return;
        }

        ClearJobHoverHighlight();
        const int notches = NormalizeMouseWheelNotches(relative);

        MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
        const bool shift =
            (input != NULL && input->isShiftPressed()) ||
            (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shift)
        {
            g_horizontalOffset = ClampInt(
                g_horizontalOffset - notches * 40,
                0,
                g_maxHorizontalOffset);
            g_changingScroll = true;
            if (g_horizontalScroll != NULL)
            {
                SetThemeScrollBarPosition(
                    g_horizontalScroll,
                    static_cast<size_t>(g_horizontalOffset));
            }
            g_changingScroll = false;
            ApplyScrollOffsets();
            StoreCurrentSquadScrollOffsets();
            if (g_drag.active && input != NULL)
            {
                UpdateDragInsertion(input->getMousePosition());
            }
            return;
        }

        g_verticalOffset = ClampInt(
            g_verticalOffset - notches * 40,
            0,
            g_maxVerticalOffset);
        g_changingScroll = true;
        if (g_verticalScroll != NULL)
        {
            SetThemeScrollBarPosition(
                g_verticalScroll,
                static_cast<size_t>(g_verticalOffset));
        }
        g_changingScroll = false;
        ApplyScrollOffsets();
        StoreCurrentSquadScrollOffsets();
    }

    void BindSquadMouseWheelTree(MyGUI::Widget* widget)
    {
        if (widget == NULL)
        {
            return;
        }

        // The bottom squad strip keeps plain wheel input routed to the member
        // rows, but owns Shift+wheel for its independent horizontal overflow.
        // Its dedicated binder handles that subtree exactly once.
        if (widget->isUserString("KJM_SquadSelectorWheelRoot"))
        {
            return;
        }

        // ScrollBar skins contain their own button/track widgets. Those
        // children forward wheel input to the native scrollbar, which would
        // otherwise move its native axis in addition to our single routed
        // Squad Jobs update. Disable the native wheel step, then bind both
        // the scrollbar and its picked skin children to our routed handler.
        if (widget->isType<MyGUI::ScrollBar>())
        {
            MyGUI::ScrollBar* scrollbar =
                widget->castType<MyGUI::ScrollBar>(false);
            if (scrollbar != NULL)
            {
                scrollbar->setScrollWheelPage(0);
            }
        }

        // MyGUI 3.2 sends wheel input only to the picked widget. It does not
        // bubble the event to a parent. Bind every current Squad Jobs child
        // once so buttons, card contents, blank panels, and scrollbars all
        // use the same vertical/Shift-horizontal routing.
        if (!widget->isUserString("KJM_SquadWheelBound"))
        {
            widget->eventMouseWheel += MyGUI::newDelegate(OnMouseWheel);
            widget->setUserString("KJM_SquadWheelBound", "1");
        }

        const size_t childCount = widget->getChildCount();
        for (size_t index = 0; index < childCount; ++index)
        {
            BindSquadMouseWheelTree(widget->getChildAt(index));
        }
    }

    void OnOptionsMouseWheel(MyGUI::Widget*, int relative)
    {
        if (g_modal.kind != MODAL_OPTIONS || g_optionsScroll == NULL ||
            relative == 0)
        {
            return;
        }

        try
        {
            const MyGUI::IntSize canvas = g_optionsScroll->getCanvasSize();
            const MyGUI::IntCoord view = g_optionsScroll->getViewCoord();
            const int maxOffset = std::max(0, canvas.height - view.height);
            // ScrollView exposes the canvas position, which is zero at the
            // top and negative while the content moves upward.  Keep the
            // arithmetic in a positive scroll-distance coordinate, then
            // convert it back for setViewOffset().
            const MyGUI::IntPoint current = g_optionsScroll->getViewOffset();
            const int currentDistance = ClampInt(
                -current.top, 0, maxOffset);
            const int nextDistance = ClampInt(
                currentDistance - NormalizeMouseWheelNotches(relative) * 40,
                0, maxOffset);
            if (nextDistance != currentDistance)
            {
                g_optionsScroll->setViewOffset(
                    MyGUI::IntPoint(current.left, -nextDistance));
            }
        }
        catch (...)
        {
            // Modal input must remain fail-closed if MyGUI is shutting down.
        }
    }

    void CloseModalNow()
    {
        g_optionsScroll = NULL;
        g_messageLogScroll = NULL;
        g_darkUiOptionButton = NULL;
        MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
        if (gui != NULL)
        {
            try
            {
                if (g_modal.window != NULL)
                {
                    MyGUI::InputManager* input =
                        MyGUI::InputManager::getInstancePtr();
                    if (input != NULL)
                    {
                        input->removeWidgetModal(g_modal.window);
                    }
                    gui->destroyWidget(g_modal.window);
                }
                if (g_modal.shade != NULL)
                {
                    gui->destroyWidget(g_modal.shade);
                }
                MyGUI::InputManager* input =
                    MyGUI::InputManager::getInstancePtr();
                if (input != NULL && g_window != NULL)
                {
                    input->setKeyFocusWidget(g_window);
                }
            }
            catch (...)
            {
                ErrorLog("[KenshiJobManagement] MyGUI threw while closing a modal.");
            }
        }
        g_modal = ModalState();
        g_stationOptionButtons.clear();
        g_modalCloseRequested = false;
    }

    bool BeginModal(ModalKind kind, int width, int height, const char* caption)
    {
        if (g_modal.kind != MODAL_NONE || g_window == NULL)
        {
            return false;
        }
        MyGUI::Gui* gui = MyGUI::Gui::getInstancePtr();
        if (gui == NULL)
        {
            return false;
        }
        // LEOPARD: sizes at the base scale, like everything inside.
        width = S(width);
        height = S(height);
        const MyGUI::IntSize view = MyGUI::RenderManager::getInstance().getViewSize();
        width = std::min(width, std::max(S(320), view.width - S(40)));
        height = std::min(height, std::max(S(180), view.height - S(40)));
        g_modal.shade = gui->createWidget<MyGUI::Widget>(
            "WhiteSkin",
            MyGUI::IntCoord(0, 0, view.width, view.height),
            MyGUI::Align::Stretch,
            "Popup",
            "KJM_ModalShade");
        g_modal.shade->setColour(MyGUI::Colour(0.0f, 0.0f, 0.0f, 0.72f));
        g_modal.shade->setNeedMouseFocus(true);

        g_modal.window = gui->createWidget<MyGUI::Window>(
            "Kenshi_WindowCX",
            MyGUI::IntCoord(
                (view.width - width) / 2,
                (view.height - height) / 2,
                width,
                height),
            MyGUI::Align::Center,
            "Popup",
            "KJM_ModalWindow");
        g_modal.window->setMovable(false);
        g_modal.window->setCaption(caption);
        g_modal.kind = kind;
        MyGUI::Widget* modalClient = g_modal.window->getClientWidget();
        if (modalClient != NULL)
        {
            const MyGUI::IntSize clientSize = modalClient->getSize();
            MyGUI::Widget* modalSurface =
                modalClient->createWidget<MyGUI::Widget>(
                    "PanelEmpty",
                    MyGUI::IntCoord(
                        0, 0, clientSize.width, clientSize.height),
                    MyGUI::Align::Stretch,
                    "KJM_ModalClientSurface");
                modalSurface->setUserString("KJM_Native", "1");   // LEOPARD: game skin, no tint
            TagThemeSurface(modalSurface);
        }
        MyGUI::InputManager::getInstance().addWidgetModal(g_modal.window);
        return true;
    }

    void OnMessageLogMouseWheel(MyGUI::Widget*, int relative)
    {
        if (g_modal.kind != MODAL_MESSAGE_LOG ||
            g_messageLogScroll == NULL || relative == 0)
        {
            return;
        }
        try
        {
            const MyGUI::IntSize canvas =
                g_messageLogScroll->getCanvasSize();
            const MyGUI::IntCoord view =
                g_messageLogScroll->getViewCoord();
            const int maximum = std::max(0, canvas.height - view.height);
            const MyGUI::IntPoint current =
                g_messageLogScroll->getViewOffset();
            const int currentDistance = ClampInt(
                -current.top, 0, maximum);
            const int nextDistance = ClampInt(
                currentDistance -
                    NormalizeMouseWheelNotches(relative) * 40,
                0, maximum);
            if (nextDistance != currentDistance)
            {
                g_messageLogScroll->setViewOffset(
                    MyGUI::IntPoint(current.left, -nextDistance));
            }
        }
        catch (...)
        {
            // The log is optional UI. A closing MyGUI tree must not affect
            // manager actions or the stored value-only messages.
        }
    }

    void OnStatusFrameClicked(MyGUI::Widget*)
    {
        if (g_window == NULL || g_modal.kind != MODAL_NONE ||
            IsStationDetailOpen() || g_drag.armed ||
            g_pendingAction.type != ACTION_NONE ||
            g_pendingGeneralMemberDrop.pending || g_pendingSquadPriority ||
            g_squadSelectorSelectionPending || HasPendingJobBatchUiAction())
        {
            return;
        }
        if (g_recentStatusMessages.empty())
        {
            ShowToast(Localize(
                "status.message_log.empty",
                "No recent messages in this session."));
            return;
        }
        const std::string modalCaption = Localize(
            "modal.message_log.title", "Recent Job Manager Messages");
        if (!BeginModal(
                MODAL_MESSAGE_LOG, 840, 540, modalCaption.c_str()))
        {
            return;
        }

        MyGUI::Widget* client = g_modal.window->getClientWidget();
        if (client == NULL)
        {
            g_modalCloseRequested = true;
            return;
        }
        const MyGUI::IntSize clientSize = client->getSize();
        MyGUI::TextBox* help = client->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText_Small",
            MyGUI::IntCoord(S(16), S(10), clientSize.width - S(32), S(26)),
            MyGUI::Align::Top | MyGUI::Align::HStretch,
            "KJM_MessageLogHelp");
        const std::string helpCaption = Localize(
            "modal.message_log.help",
            "The 10 most recent non-empty timestamped messages from this game session (oldest to newest).");
        help->setCaption(helpCaption.c_str());
        SetFontPx(help, S(15));
        help->setTextAlign(MyGUI::Align::Center);
        help->setNeedMouseFocus(false);
        TagThemeStandardText(help);

        std::ostringstream historyCaption;
        size_t historyLineCount = 0;
        const size_t messageWrapCharacters = static_cast<size_t>(
            std::min(
                86,
                std::max(24, (clientSize.width - 76) / 8)));
        for (size_t index = 0;
             index < g_recentStatusMessages.size(); ++index)
        {
            if (index != 0)
            {
                historyCaption << "\n\n";
                ++historyLineCount;
            }
            std::ostringstream numberedMessage;
            numberedMessage << (index + 1) << ". " <<
                "[" << g_recentStatusMessages[index].timestamp << "] " <<
                g_recentStatusMessages[index].text;
            size_t messageLineCount = 0;
            historyCaption << WrapToolTipCaption(
                numberedMessage.str(), messageWrapCharacters,
                &messageLineCount);
            historyLineCount += messageLineCount;
        }

        // The log owns the only visible nested ScrollView scrollbar. Use
        // Kenshi's large-slider ScrollView layout in Dark UI mode because
        // MyGUI deliberately keeps nested skin scrollbars private.
        const char* messageLogScrollSkin =
            IsDarkUIFriendlyFontColorsEnabled() ?
                "Kenshi_ScrollViewEmptyLight" : "Kenshi_ScrollView";
        const MyGUI::IntCoord messageLogScrollCoord(
            16, 44, clientSize.width - 32,
            std::max(100, clientSize.height - 104));
        MyGUI::ScrollView* scroll = client->createWidget<MyGUI::ScrollView>(
            messageLogScrollSkin,
            messageLogScrollCoord,
            MyGUI::Align::Stretch,
            "KJM_MessageLogScroll");
        g_messageLogScroll = scroll;
        scroll->setVisibleHScroll(false);
        scroll->eventMouseWheel +=
            MyGUI::newDelegate(OnMessageLogMouseWheel);
        const MyGUI::IntCoord view = scroll->getViewCoord();
        const int canvasWidth = std::max(1, view.width - S(8));
        const int canvasHeight = std::max(
            view.height,
            static_cast<int>(historyLineCount) * 22 + 20);
        scroll->setCanvasSize(canvasWidth, canvasHeight);
        const bool showVerticalScroll = canvasHeight > view.height;
        scroll->setVisibleVScroll(showVerticalScroll);

        // ScrollView keeps its skin scrollbars private. Its large native skin
        // supplies the track and handle; add the same dark outline as the
        // explicit manager bars around its 32-pixel slider column. Do not add
        // a foreground track overlay here because it could show through the
        // private translucent handle.
        if (showVerticalScroll && IsDarkUIFriendlyFontColorsEnabled())
        {
            const int scrollBarWidth = S(32);
            const int scrollBarLeft =
                messageLogScrollCoord.left +
                messageLogScrollCoord.width - scrollBarWidth;
            const int scrollBarTop = messageLogScrollCoord.top;
            const int scrollBarHeight = messageLogScrollCoord.height;
            CreateThemeScrollBarEdge(
                client,
                MyGUI::IntCoord(
                    scrollBarLeft, scrollBarTop,
                    scrollBarWidth, S(2)),
                MyGUI::Align::Top);
            CreateThemeScrollBarEdge(
                client,
                MyGUI::IntCoord(
                    scrollBarLeft,
                    scrollBarTop + std::max(0, scrollBarHeight - S(2)),
                    scrollBarWidth,
                    S(2)),
                MyGUI::Align::Top);
            CreateThemeScrollBarEdge(
                client,
                MyGUI::IntCoord(
                    scrollBarLeft, scrollBarTop,
                    S(2), scrollBarHeight),
                MyGUI::Align::Top);
            CreateThemeScrollBarEdge(
                client,
                MyGUI::IntCoord(
                    scrollBarLeft + scrollBarWidth - S(2),
                    scrollBarTop,
                    S(2),
                    scrollBarHeight),
                MyGUI::Align::Top);
        }

        MyGUI::Widget* historySurface = scroll->createWidget<MyGUI::Widget>(
            "Kenshi_GenericTextBoxFlatSkin",
            MyGUI::IntCoord(0, 0, canvasWidth, canvasHeight),
            MyGUI::Align::Left | MyGUI::Align::Top,
            "KJM_MessageLogSurface");
        historySurface->setUserString("KJM_Native", "1");   // LEOPARD: game skin, no tint
        TagThemeStatusFrame(historySurface);
        historySurface->setNeedMouseFocus(true);
        historySurface->eventMouseWheel +=
            MyGUI::newDelegate(OnMessageLogMouseWheel);

        MyGUI::TextBox* history = scroll->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(S(14), S(10), canvasWidth - S(28), canvasHeight - S(20)),
            MyGUI::Align::Left | MyGUI::Align::Top,
            "KJM_MessageLogText");
        history->setCaption(historyCaption.str().c_str());
        SetFontPx(history, S(16));
        history->setTextAlign(MyGUI::Align::Left | MyGUI::Align::Top);
        history->setNeedMouseFocus(false);
        TagThemeStandardText(history);

        MyGUI::Button* close = client->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(
                clientSize.width - S(156), clientSize.height - S(50), S(140), S(38)),
            MyGUI::Align::Right | MyGUI::Align::Bottom,
            "KJM_MessageLogClose");
        const std::string closeCaption = Localize(
            "ui.action.close", "Close");
        SetFittedButtonCaption(close, closeCaption, 16, 10, 14);
        close->eventMouseButtonClick += MyGUI::newDelegate(OnOptionClose);
        TagThemeButtonText(close);
        ApplyThemeToTaggedTree(g_modal.window);
        MyGUI::InputManager::getInstance().setKeyFocusWidget(close);
    }

    void OpenClearModal(int memberIndex)
    {
        const MemberSnapshot* memberPointer = memberIndex >= 0 ?
            GetVisibleMember(memberIndex) : NULL;
        if (memberPointer == NULL)
        {
            return;
        }
        const MemberSnapshot& member = *memberPointer;
        const std::string modalCaption = Localize(
            "modal.clear.title", "Confirm Clear Queue");
        if (!member.queueAvailable || member.jobs.empty() ||
            !BeginModal(MODAL_CLEAR, 520, 210, modalCaption.c_str()))
        {
            return;
        }

        g_modal.member = member.identity;
        g_modal.reviewedQueue = member.jobs;
        MyGUI::Widget* client = g_modal.window->getClientWidget();
        MyGUI::TextBox* prompt = client->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(S(18), S(18), S(484), S(78)),
            MyGUI::Align::Top | MyGUI::Align::HStretch,
            "KJM_ClearPrompt");
        const std::string promptCaption = LocalizeFormat(
            "modal.clear.prompt",
            "Are you sure you wish to remove all jobs?\n\nRemove all {count} permanent jobs from {member}?",
            "{count}", IntegerString(member.jobs.size()),
            "{member}", member.name);
        prompt->setCaption(promptCaption.c_str());
        prompt->setTextAlign(MyGUI::Align::Center);
        prompt->setNeedMouseFocus(false);
        TagThemeStandardText(prompt);

        MyGUI::Button* yes = client->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(S(88), S(115), S(150), S(38)),
            MyGUI::Align::Left | MyGUI::Align::Bottom,
            "KJM_ClearYes");
        const std::string yesCaption = Localize(
            "ui.dialog.yes", "Yes");
        SetFittedButtonCaption(yes, yesCaption, 16, 10, 14);
        yes->eventMouseButtonClick += MyGUI::newDelegate(OnClearYes);
        TagThemeButtonText(yes);
        MyGUI::Button* no = client->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(S(282), S(115), S(150), S(38)),
            MyGUI::Align::Right | MyGUI::Align::Bottom,
            "KJM_ClearNo");
        const std::string noCaption = Localize(
            "ui.dialog.no", "No");
        SetFittedButtonCaption(no, noCaption, 16, 10, 14);
        no->eventMouseButtonClick += MyGUI::newDelegate(OnClearNo);
        TagThemeButtonText(no);
        ApplyThemeToTaggedTree(g_modal.window);
        MyGUI::InputManager::getInstance().setKeyFocusWidget(no);
    }

    void OnClearYes(MyGUI::Widget*)
    {
        if (g_modal.kind != MODAL_CLEAR || g_pendingAction.type != ACTION_NONE)
        {
            return;
        }
        g_pendingAction = PendingAction();
        g_pendingAction.type = ACTION_CLEAR_MEMBER;
        g_pendingAction.member = g_modal.member;
        g_pendingAction.sequence = g_modal.reviewedQueue;
        g_modalCloseRequested = true;
    }

    void OnClearNo(MyGUI::Widget*)
    {
        g_modalCloseRequested = true;
    }

    void RefreshOptionButtons()
    {
        if (g_darkUiOptionButton != NULL)
        {
            g_darkUiOptionButton->setStateSelected(
                IsDarkUIFriendlyFontColorsEnabled());
            ApplyThemeButtonText(g_darkUiOptionButton);
        }
        for (size_t index = 0; index < g_stationOptionButtons.size(); ++index)
        {
            g_stationOptionButtons[index].button->setStateSelected(
                IsStationCategoryEnabled(
                g_stationOptionButtons[index].category));
            ApplyThemeButtonText(
                g_stationOptionButtons[index].button);
        }
    }

    void OnDarkUiOptionClicked(MyGUI::Widget*)
    {
        const bool enabled = !IsDarkUIFriendlyFontColorsEnabled();
        const bool saved = SetDarkUIFriendlyFontColors(enabled);
        ApplyThemeToTaggedTree(g_window);
        ApplyThemeToTaggedTree(g_modal.window);
        ApplyThemeToTaggedTree(g_tooltip);
        UpdateSquadSelectorSelection();
        RefreshOptionButtons();
        if (!saved)
        {
            SetStatus(Localize(
                "status.settings.appearance_save_failed",
                "Appearance changed for this session, but settings.ini could not be saved."));
        }
    }

    void OnStationCategoryOptionClicked(MyGUI::Widget* widget)
    {
        for (size_t index = 0; index < g_stationOptionButtons.size(); ++index)
        {
            StationCategoryOptionButton& binding =
                g_stationOptionButtons[index];
            if (binding.button != widget)
            {
                continue;
            }
            const int category = static_cast<int>(binding.category);
            g_stationCategoryEnabled[category] =
                !g_stationCategoryEnabled[category];
            if (!SaveStationCategorySettings())
            {
                g_settingsWriteFailed = true;
            }
            RefreshOptionButtons();
            g_stationFilterRefreshRequested = true;
            return;
        }
    }

    void OnOptionReset(MyGUI::Widget*)
    {
        ResetDefaultStationCategories();
        if (!SaveStationCategorySettings())
        {
            g_settingsWriteFailed = true;
        }
        ApplyThemeToTaggedTree(g_window);
        ApplyThemeToTaggedTree(g_modal.window);
        ApplyThemeToTaggedTree(g_tooltip);
        UpdateSquadSelectorSelection();
        g_stationFilterRefreshRequested = true;
        RefreshOptionButtons();
    }

    void OnOptionClose(MyGUI::Widget*)
    {
        g_modalCloseRequested = true;
    }

    void OpenOptionsModal()
    {
        // Give the two-column filter list enough vertical room to show every
        // option at once. The canvas remains a ScrollView for compatibility
        // with existing input routing, but it no longer needs a scrollbar at
        // the normal supported resolutions.
        const std::string modalCaption = Localize(
            "modal.options.title", "Job Manager Options");
        if (!BeginModal(MODAL_OPTIONS, 900, 440, modalCaption.c_str()))
        {
            return;
        }
        MyGUI::Widget* client = g_modal.window->getClientWidget();
        const MyGUI::IntSize clientSize = client->getSize();
        MyGUI::TextBox* help = client->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText_Small",
            MyGUI::IntCoord(S(16), S(8), clientSize.width - S(32), S(44)),
            MyGUI::Align::Top | MyGUI::Align::HStretch,
            "KJM_OptionsHelp");
        SetFontPx(help, S(15));
        const std::string helpCaption = Localize(
            "modal.options.help",
            "Which station categories the Stations tab shows.\nChanges are saved and apply immediately.");
        help->setCaption(helpCaption.c_str());
        help->setTextAlign(MyGUI::Align::Center);
        help->setNeedMouseFocus(false);
        TagThemeStandardText(help);

        MyGUI::ScrollView* scroll = client->createWidget<MyGUI::ScrollView>(
            "Kenshi_ScrollView",
            MyGUI::IntCoord(
                S(16),
                S(56),
                clientSize.width - S(32),
                std::max(S(180), clientSize.height - S(122))),
            MyGUI::Align::Stretch,
            "KJM_OptionsScroll");
        scroll->setVisibleVScroll(false);
        scroll->setVisibleHScroll(false);
        scroll->eventMouseWheel += MyGUI::newDelegate(OnOptionsMouseWheel);
        g_optionsScroll = scroll;

        const int canvasWidth = std::max(S(240), clientSize.width - S(60));
        const int columnGap = S(16);
        const int columnWidth = std::max(
            S(100), (canvasWidth - S(16) - columnGap) / 2);
        const int leftColumn = S(8);
        const int rightColumn = leftColumn + columnWidth + columnGap;

        // LEOPARD: the "Appearance" section (Dark UI switch) is gone - the
        // colours follow the game's interface theme.
        g_darkUiOptionButton = NULL;
        MyGUI::TextBox* stationSection = scroll->createWidget<MyGUI::TextBox>(
            "Kenshi_TextboxStandardText",
            MyGUI::IntCoord(leftColumn, S(8), canvasWidth, S(30)),
            MyGUI::Align::Left | MyGUI::Align::Top,
            "KJM_StationFilterSection");
        const std::string stationSectionCaption = Localize(
            "options.stations.header", "STATION CATEGORY FILTERS");
        stationSection->setCaption(stationSectionCaption.c_str());
        stationSection->setNeedMouseFocus(false);
        TagThemeStandardText(stationSection);

        for (size_t index = 0;
             index < STATION_CATEGORY_DEFINITION_COUNT; ++index)
        {
            const StationCategoryDefinition& definition =
                STATION_CATEGORY_DEFINITIONS[index];
            const size_t column = index / 5;
            const size_t row = index % 5;
            const int x = column == 0 ? leftColumn : rightColumn;
            const int y = S(46) + static_cast<int>(row) * S(40);
            MyGUI::Button* category = scroll->createWidget<MyGUI::Button>(
                "Kenshi_TickButton1",
                MyGUI::IntCoord(
                    x, y + (S(34) - 26) / 2, columnWidth, 26),
                MyGUI::Align::Left | MyGUI::Align::Top,
                "KJM_StationCategoryOption");
            const std::string categoryCaption = Localize(
                definition.localizationKey, definition.name);
            SetFittedButtonCaption(
                category, categoryCaption, 15, 10, 20);
            // The skin's lamp stretches with the height: native 26 px tall,
            // so the caption font stays at most the 20 px game font.
            if (category->getFontHeight() > 20)
            {
                SetFontPx(category, 20);
            }
            category->eventMouseButtonClick +=
                MyGUI::newDelegate(OnStationCategoryOptionClicked);
            category->eventMouseWheel +=
                MyGUI::newDelegate(OnOptionsMouseWheel);
            TagThemeButtonText(category);
            StationCategoryOptionButton binding;
            binding.button = category;
            binding.category = definition.category;
            g_stationOptionButtons.push_back(binding);
        }
        scroll->setCanvasSize(
            canvasWidth, S(46) + 5 * S(40) + S(8));
        MyGUI::Widget* optionsSurface = scroll->createWidget<MyGUI::Widget>(
            "PanelEmpty",
            MyGUI::IntCoord(0, 0, canvasWidth, S(46) + 5 * S(40) + S(8)),
            MyGUI::Align::Left | MyGUI::Align::Top,
            "KJM_OptionsSurface");
        optionsSurface->setUserString("KJM_Native", "1");   // LEOPARD: game skin, no tint
        TagThemeSurface(optionsSurface);

        MyGUI::Button* reset = client->createWidget<MyGUI::Button>(
            "Kenshi_Button1", MyGUI::IntCoord(S(16), clientSize.height - S(50), S(160), S(38)),
            MyGUI::Align::Left | MyGUI::Align::Bottom, "KJM_OptionReset");
        const std::string resetCaption = Localize(
            "ui.action.reset_default", "Reset Default");
        SetFittedButtonCaption(reset, resetCaption, 16, 10, 14);
        reset->eventMouseButtonClick += MyGUI::newDelegate(OnOptionReset);
        TagThemeButtonText(reset);
        MyGUI::Button* close = client->createWidget<MyGUI::Button>(
            "Kenshi_Button1",
            MyGUI::IntCoord(clientSize.width - S(156), clientSize.height - S(50), S(140), S(38)),
            MyGUI::Align::Right | MyGUI::Align::Bottom, "KJM_OptionClose");
        const std::string closeCaption = Localize(
            "ui.action.close", "Close");
        SetFittedButtonCaption(close, closeCaption, 16, 10, 14);
        close->eventMouseButtonClick += MyGUI::newDelegate(OnOptionClose);
        TagThemeButtonText(close);
        RefreshOptionButtons();
        ApplyThemeToTaggedTree(g_modal.window);
        MyGUI::InputManager::getInstance().setKeyFocusWidget(close);
    }

    void OnRecipientPortraitClicked(MyGUI::Widget* widget)
    {
        if (g_window == NULL || g_stationTabActive ||
            g_modal.kind != MODAL_NONE || IsStationDetailOpen() ||
            g_drag.armed || g_pendingAction.type != ACTION_NONE ||
            g_pendingGeneralMemberDrop.pending || g_pendingSquadPriority ||
            HasPendingJobBatchUiAction())
        {
            return;
        }

        int memberIndex = -1;
        if (!GetWidgetIndex(widget, "KJM_Member", &memberIndex) ||
            memberIndex < 0 ||
            memberIndex >= static_cast<int>(g_visibleMemberBindings.size()))
        {
            return;
        }
        const MemberSnapshot* member = GetVisibleMember(memberIndex);
        if (member == NULL || !member->queueAvailable)
        {
            return;
        }

        // Entering recipient mode clears the job-card mode while preserving
        // the existing recipient set so Ctrl can still toggle one person.
        ResetBottomSquadSelectionTracking();
        ClearJobSelectionForRecipientInteraction();

        MyGUI::InputManager* input = MyGUI::InputManager::getInstancePtr();
        const bool control =
            (input != NULL && input->isControlPressed()) ||
            (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
        const HandleIdentity squad =
            g_visibleMemberBindings[memberIndex].squad;
        if (control)
        {
            ToggleSelectedRecipient(squad, member->identity);
        }
        else
        {
            SelectOnlyRecipient(squad, member->identity);
        }
        ApplyRecipientSelectionStates();
        UpdateSquadSelectorSelection();
        UpdateSquadHeading();
    }

    void OnJobsToggleClicked(MyGUI::Widget* widget)
    {
        int memberIndex = -1;
        if (!GetWidgetIndex(widget, "KJM_Member", &memberIndex) ||
            memberIndex < 0 ||
            g_pendingAction.type != ACTION_NONE)
        {
            return;
        }
        const MemberSnapshot* member = GetVisibleMember(memberIndex);
        if (member == NULL || !member->queueAvailable)
        {
            return;
        }
        g_pendingAction = PendingAction();
        g_pendingAction.type = ACTION_TOGGLE_JOBS;
        g_pendingAction.member = member->identity;
    }

    void OnClearClicked(MyGUI::Widget* widget)
    {
        int memberIndex = -1;
        if (GetWidgetIndex(widget, "KJM_Member", &memberIndex))
        {
            OpenClearModal(memberIndex);
        }
    }

    void OnRemoveClicked(MyGUI::Widget*)
    {
        QueueRemoveSelectedAction();
    }

    void OnSquadGroupToggle(MyGUI::Widget* widget)
    {
        int squadIndex = -1;
        if (!GetWidgetIndex(widget, "KJM_SquadGroup", &squadIndex) ||
            squadIndex < 0 ||
            squadIndex >= static_cast<int>(g_allSquads.squads.size()))
        {
            return;
        }
        CancelDrag();
        const HandleIdentity identity =
            g_allSquads.squads[squadIndex].identity;
        SetSquadCollapsed(identity, !IsSquadCollapsed(identity));
        // The clicked header belongs to the tree that rebuild destroys.
        // Rebuild only after this MyGUI callback returns.
        g_squadGroupRebuildRequested = true;
    }

    void OnPrioritizeCoreJobsClicked(MyGUI::Widget*)
    {
        if (g_allSquads.incomplete || g_pendingSquadPriority ||
            g_pendingGeneralMemberDrop.pending ||
            HasPendingJobBatchUiAction() ||
            g_pendingAction.type != ACTION_NONE || g_drag.armed)
        {
            return;
        }
        if (!RecipientSelectionMode())
        {
            ShowToast(Localize(
                "status.selection.no_recipient",
                "Select at least one recipient first."));
            return;
        }
        const std::vector<HandleIdentity> recipients =
            GetSelectedRecipientIdentities();
        if (recipients.empty())
        {
            ShowToast(Localize(
                "status.selection.no_recipient",
                "Select at least one recipient first."));
            return;
        }
        g_pendingJobBatchUiAction = PendingJobBatchUiAction();
        g_pendingJobBatchUiAction.type =
            JOB_BATCH_UI_PRIORITIZE_HEALING;
        g_pendingJobBatchUiAction.recipients = recipients;
        SetStatus(Localize(
            "status.batch.applying_healing_priorities",
            "Applying healing priorities..."));
    }

    void OnAddHealingJobsClicked(MyGUI::Widget*)
    {
        if (g_allSquads.incomplete || g_pendingSquadPriority ||
            g_pendingGeneralMemberDrop.pending ||
            HasPendingJobBatchUiAction() ||
            g_pendingAction.type != ACTION_NONE || g_drag.armed)
        {
            return;
        }
        if (!RecipientSelectionMode())
        {
            ShowToast(Localize(
                "status.selection.no_recipient",
                "Select at least one recipient first."));
            return;
        }
        const std::vector<HandleIdentity> recipients =
            GetSelectedRecipientIdentities();
        if (recipients.empty())
        {
            ShowToast(Localize(
                "status.selection.no_recipient",
                "Select at least one recipient first."));
            return;
        }
        g_pendingJobBatchUiAction = PendingJobBatchUiAction();
        g_pendingJobBatchUiAction.type = JOB_BATCH_UI_ADD_HEALING;
        g_pendingJobBatchUiAction.recipients = recipients;
        SetStatus(Localize(
            "status.batch.adding_healing_jobs",
            "Adding and prioritizing healing jobs..."));
    }

    void OnRemoveInvalidJobsClicked(MyGUI::Widget*)
    {
        if (g_allSquads.incomplete || g_pendingSquadPriority ||
            g_pendingGeneralMemberDrop.pending ||
            HasPendingJobBatchUiAction() ||
            g_pendingAction.type != ACTION_NONE || g_drag.armed)
        {
            return;
        }
        if (!RecipientSelectionMode())
        {
            ShowToast(Localize(
                "status.selection.no_recipient",
                "Select at least one recipient first."));
            return;
        }
        const std::vector<HandleIdentity> recipients =
            GetSelectedRecipientIdentities();
        if (recipients.empty())
        {
            ShowToast(Localize(
                "status.selection.no_recipient",
                "Select at least one recipient first."));
            return;
        }
        g_pendingJobBatchUiAction = PendingJobBatchUiAction();
        g_pendingJobBatchUiAction.type = JOB_BATCH_UI_REMOVE_INVALID;
        g_pendingJobBatchUiAction.recipients = recipients;
        SetStatus(Localize(
            "status.batch.removing_invalid_jobs",
            "Removing invalid fixed-target jobs..."));
    }

    void OnOptionsClicked(MyGUI::Widget*)
    {
        OpenOptionsModal();
    }

    struct RemovalCandidate
    {
        SelectedJob selected;
        int memberIndex;
        int slot;
    };

    bool RemovalCandidateLess(
        const RemovalCandidate& left,
        const RemovalCandidate& right)
    {
        if (left.memberIndex != right.memberIndex)
        {
            return left.memberIndex < right.memberIndex;
        }
        return left.slot > right.slot;
    }

    std::string DescribeRemovalCandidate(const RemovalCandidate& candidate)
    {
        std::string memberName;
        const MemberSnapshot* member = FindDisplayedMember(
            candidate.selected.member);
        if (member != NULL)
        {
            memberName = member->name;
        }
        else
        {
            memberName = Localize(
                "member.label.unavailable", "Unavailable member");
        }
        if (candidate.slot >= 0)
        {
            return LocalizeFormat(
                "status.remove_selected.candidate_with_priority",
                "{member} / {job} / Priority {priority}",
                "{member}", memberName,
                "{job}", candidate.selected.job.jobLabel,
                "{priority}", IntegerString(candidate.slot + 1));
        }
        return LocalizeFormat(
            "status.remove_selected.candidate",
            "{member} / {job}",
            "{member}", memberName,
            "{job}", candidate.selected.job.jobLabel);
    }

    void EraseSelectedJob(const SelectedJob& completed)
    {
        const int index = FindSelectedJobIndex(
            completed.member,
            completed.job,
            completed.lastSlot);
        if (index >= 0)
        {
            g_selectedJobs.erase(g_selectedJobs.begin() + index);
            SyncRecipientsToSelectedJobs();
        }
    }

    bool TryBuildLiveMemberByIdentity(
        const HandleIdentity& identity,
        MemberSnapshot* memberOut);
    bool TryBuildFreshRosterMemberByIdentity(
        const HandleIdentity& identity,
        MemberSnapshot* memberOut);

    void ProcessRemoveSelected()
    {
        std::vector<RemovalCandidate> candidates;
        for (size_t index = 0; index < g_selectedJobs.size(); ++index)
        {
            const int memberIndex = FindDisplayedMemberOrder(
                g_selectedJobs[index].member);
            int slot = -1;
            if (memberIndex >= 0)
            {
                const MemberSnapshot* member = FindDisplayedMember(
                    g_selectedJobs[index].member);
                if (member != NULL)
                {
                    slot = FindJobSlot(
                        *member,
                        g_selectedJobs[index].job);
                    if (g_selectedJobs[index].job.taskToken == 0)
                    {
                        slot = g_selectedJobs[index].lastSlot;
                    }
                    else if (slot < 0)
                    {
                        slot = g_selectedJobs[index].lastSlot;
                    }
                }
            }
            RemovalCandidate candidate;
            candidate.selected = g_selectedJobs[index];
            candidate.memberIndex = memberIndex;
            candidate.slot = slot;
            candidates.push_back(candidate);
        }
        std::sort(candidates.begin(), candidates.end(), RemovalCandidateLess);

        size_t completedCount = 0;
        bool interrupted = false;
        std::string failedDetail;
        for (size_t index = 0; index < candidates.size(); ++index)
        {
            RemovalCandidate& candidate = candidates[index];
            if (candidate.memberIndex < 0 || candidate.slot < 0)
            {
                interrupted = true;
                failedDetail = DescribeRemovalCandidate(candidate);
                break;
            }
            MemberSnapshot fresh;
            if (!TryBuildFreshRosterMemberByIdentity(
                    candidate.selected.member, &fresh))
            {
                interrupted = true;
                failedDetail = DescribeRemovalCandidate(candidate);
                break;
            }

            int liveSlot = FindJobSlot(fresh, candidate.selected.job);
            if (candidate.selected.job.taskToken == 0)
            {
                liveSlot = candidate.slot;
                if (liveSlot < 0 || liveSlot >= static_cast<int>(fresh.jobs.size()) ||
                    !SameJob(fresh.jobs[liveSlot], candidate.selected.job))
                {
                    liveSlot = -1;
                }
            }
            if (liveSlot < 0 || !TryRemovePermajob(fresh.handle, liveSlot))
            {
                interrupted = true;
                failedDetail = DescribeRemovalCandidate(candidate);
                break;
            }
            EraseSelectedJob(candidate.selected);
            ++completedCount;
        }

        const std::vector<SelectedJob> remaining = g_selectedJobs;
        if (completedCount > 0)
        {
            TryNotifyVanillaSelectionUI();
        }
        RefreshSquadView(true);
        if (interrupted)
        {
            // Keep the failed and not-yet-attempted rows selected, including
            // logical selections that became temporarily unavailable.
            for (size_t index = 0; index < remaining.size(); ++index)
            {
                if (FindSelectedJobIndex(
                        remaining[index].member,
                        remaining[index].job,
                        remaining[index].lastSlot) < 0)
                {
                    g_selectedJobs.push_back(remaining[index]);
                }
            }
            ApplySynchronizedJobSelectionStates();
        }
        if (interrupted)
        {
            const std::string status = LocalizeFormat(
                "status.remove_selected.interrupted",
                "Deletion queue interrupted. Completed removals: {completed}. Failed: {failed}. Remaining selected jobs: {remaining}. Review and try again.",
                "{completed}", IntegerString(completedCount),
                "{failed}", failedDetail,
                "{remaining}", IntegerString(remaining.size()));
            SetStatus(status);
        }
        else
        {
            SetStatus(LocalizeFormat(
                "status.remove_selected.success",
                "Removed permanent jobs: {count}.",
                "{count}", IntegerString(completedCount)));
        }
    }

    bool SameStationJobKindAndTarget(
        const JobRowSnapshot& job,
        TaskType taskType,
        const HandleIdentity& target)
    {
        return job.taskType == taskType && job.hasTarget &&
            SameHandleIdentity(job.target, target);
    }

    bool HasStationJobKindAndTarget(
        const std::vector<JobRowSnapshot>& jobs,
        TaskType taskType,
        const HandleIdentity& target)
    {
        for (size_t index = 0; index < jobs.size(); ++index)
        {
            if (SameStationJobKindAndTarget(jobs[index], taskType, target))
            {
                return true;
            }
        }
        return false;
    }

    bool TryBuildLiveMemberByIdentity(
        const HandleIdentity& identity,
        MemberSnapshot* memberOut)
    {
        if (memberOut == NULL || !identity.valid ||
            identity.type != CHARACTER)
        {
            return false;
        }
        const hand member = RestoreHandleIdentity(identity);
        MemberSnapshot fresh;
        if (!BuildMemberSnapshot(member, &fresh) ||
            !fresh.loaded || !fresh.queueAvailable || fresh.truncated ||
            !SameHandleIdentity(fresh.identity, identity))
        {
            return false;
        }
        *memberOut = fresh;
        return true;
    }

    bool TryBuildFreshRosterMemberByIdentity(
        const HandleIdentity& identity,
        MemberSnapshot* memberOut)
    {
        if (memberOut == NULL ||
            !RefreshAllActiveSquadsSnapshot())
        {
            return false;
        }
        const MemberSnapshot* rosterMember =
            FindDisplayedMember(identity);
        if (rosterMember == NULL || !rosterMember->loaded ||
            !rosterMember->queueAvailable || rosterMember->truncated)
        {
            return false;
        }
        return TryBuildLiveMemberByIdentity(identity, memberOut);
    }

    bool TryAddStationPermanentJob(
        const hand& member,
        TaskType taskType,
        const HandleIdentity& targetIdentity)
    {
        const hand target = RestoreHandleIdentity(targetIdentity);
        Building* building = NULL;
        if (targetIdentity.type != BUILDING ||
            !TryResolveStationBuilding(target, &building) || building == NULL)
        {
            return false;
        }
        bool playerManaged = false;
        if (!TryIsPlayerManagedStation(building, true, &playerManaged) ||
            !playerManaged)
        {
            return false;
        }

        __try
        {
            if (!member || !member.isValid())
            {
                return false;
            }
            Character* character = member.getCharacter();
            if (character == NULL)
            {
                return false;
            }
            const Ogre::Vector3 position = building->getPosition();
            // These flags reproduce Kenshi's permanent shift-add path. The
            // destination queue is appended without clearing existing jobs.
            character->addJob(taskType, building, true, true, position);
            character->reThinkCurrentAIAction();
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    bool SameQueueWithAppendedStationJob(
        const std::vector<JobRowSnapshot>& before,
        const std::vector<JobRowSnapshot>& after,
        TaskType taskType,
        const HandleIdentity& target)
    {
        if (after.size() != before.size() + 1)
        {
            return false;
        }
        for (size_t index = 0; index < before.size(); ++index)
        {
            if (!SameJob(before[index], after[index]) ||
                before[index].jobLabel != after[index].jobLabel ||
                before[index].targetLabel != after[index].targetLabel ||
                before[index].hasTarget != after[index].hasTarget ||
                before[index].targetAvailable != after[index].targetAvailable)
            {
                return false;
            }
        }
        return SameStationJobKindAndTarget(after.back(), taskType, target);
    }

    bool SameQueueWithExactJobRemoved(
        const std::vector<JobRowSnapshot>& before,
        int removedSlot,
        const std::vector<JobRowSnapshot>& after)
    {
        if (removedSlot < 0 ||
            removedSlot >= static_cast<int>(before.size()) ||
            after.size() + 1 != before.size())
        {
            return false;
        }
        for (size_t index = 0; index < after.size(); ++index)
        {
            const size_t beforeIndex = index < static_cast<size_t>(removedSlot) ?
                index : index + 1;
            if (!SameJob(after[index], before[beforeIndex]) ||
                after[index].jobLabel != before[beforeIndex].jobLabel ||
                after[index].targetLabel != before[beforeIndex].targetLabel ||
                after[index].hasTarget != before[beforeIndex].hasTarget ||
                after[index].targetAvailable !=
                    before[beforeIndex].targetAvailable)
            {
                return false;
            }
        }
        return true;
    }

    void SetStationActionStatus(const std::string& status)
    {
        SetStatus(status);
        SetStationDetailStatus(status);
    }

    bool TryCaptureProjectedStationMemberQueue(
        const HandleIdentity& station,
        const HandleIdentity& member,
        std::vector<JobRowSnapshot>* queueOut)
    {
        if (queueOut == NULL || !station.valid || station.type != BUILDING ||
            !member.valid || member.type != CHARACTER ||
            !g_stationScan.started)
        {
            return false;
        }
        queueOut->clear();

        bool stationFound = false;
        for (size_t stationIndex = 0;
             stationIndex < g_stationScan.stations.size(); ++stationIndex)
        {
            if (SameHandleIdentity(
                    g_stationScan.stations[stationIndex].identity, station))
            {
                if (stationFound)
                {
                    return false;
                }
                stationFound = true;
            }
        }
        if (!stationFound)
        {
            return false;
        }

        const StationMemberSnapshot* projectedMember = NULL;
        for (size_t squadIndex = 0;
             squadIndex < g_stationScan.squads.size(); ++squadIndex)
        {
            const StationSquadSnapshot& squad =
                g_stationScan.squads[squadIndex];
            for (size_t memberIndex = 0;
                 memberIndex < squad.members.size(); ++memberIndex)
            {
                const StationMemberSnapshot& candidate =
                    squad.members[memberIndex];
                if (!SameHandleIdentity(candidate.identity, member))
                {
                    continue;
                }
                if (projectedMember != NULL)
                {
                    return false;
                }
                projectedMember = &candidate;
            }
        }
        if (projectedMember == NULL || !projectedMember->loaded ||
            !projectedMember->queueAvailable || projectedMember->truncated ||
            projectedMember->permanentJobCount < 0 ||
            static_cast<size_t>(projectedMember->permanentJobCount) !=
                projectedMember->jobs.size() ||
            projectedMember->jobs.size() >
                static_cast<size_t>(MAX_SAFE_JOB_ROWS))
        {
            return false;
        }

        queueOut->reserve(projectedMember->jobs.size());
        for (size_t index = 0; index < projectedMember->jobs.size(); ++index)
        {
            queueOut->push_back(projectedMember->jobs[index].exactJob);
        }
        return true;
    }

    bool TryGetAssignedNaturalAddAuthorization(
        const HandleIdentity& station,
        bool* allowAssignedNaturalExceptionOut)
    {
        if (allowAssignedNaturalExceptionOut == NULL ||
            !station.valid || station.type != BUILDING ||
            !g_stationScan.started)
        {
            return false;
        }
        *allowAssignedNaturalExceptionOut = false;

        const StationTargetSnapshot* matchedStation = NULL;
        for (size_t stationIndex = 0;
             stationIndex < g_stationScan.stations.size(); ++stationIndex)
        {
            const StationTargetSnapshot& candidate =
                g_stationScan.stations[stationIndex];
            if (!SameHandleIdentity(candidate.identity, station))
            {
                continue;
            }
            if (matchedStation != NULL)
            {
                return false;
            }
            matchedStation = &candidate;
        }
        if (matchedStation == NULL)
        {
            return false;
        }

        // A non-player natural node is actionable only while a readable
        // loaded-player queue still proves that Kenshi already assigned it.
        // Player-owned buildings do not need this exception; the guarded
        // native leaf checks their ownership directly.
        *allowAssignedNaturalExceptionOut =
            matchedStation->naturalResourceException &&
            !matchedStation->assignments.empty();
        return true;
    }

    void RequestAddStationAssignment(
        const HandleIdentity& station,
        const HandleIdentity& member)
    {
        if (g_pendingAction.type != ACTION_NONE)
        {
            SetStationActionStatus(Localize(
                "status.station_action.pending",
                "Another job change is pending. Try again after it finishes."));
            return;
        }

        std::vector<JobRowSnapshot> capturedQueue;
        if (!TryCaptureProjectedStationMemberQueue(
                station, member, &capturedQueue))
        {
            SetStationActionStatus(Localize(
                "status.station_action.queue_or_station_changed",
                "The member queue or station changed. No jobs were changed."));
            return;
        }

        g_pendingAction = PendingAction();
        g_pendingAction.type = ACTION_ASSIGN_STATION;
        g_pendingAction.member = member;
        g_pendingAction.stationTarget = station;
        g_pendingAction.sequence = capturedQueue;
        SetStationActionStatus(Localize(
            "status.station_action.validating_assignment",
            "Validating the station assignment..."));
    }

    void RequestRemoveStationAssignment(
        const HandleIdentity& station,
        const HandleIdentity& member)
    {
        if (g_pendingAction.type != ACTION_NONE)
        {
            SetStationActionStatus(Localize(
                "status.station_action.pending",
                "Another job change is pending. Try again after it finishes."));
            return;
        }

        std::vector<JobRowSnapshot> capturedQueue;
        if (!TryCaptureProjectedStationMemberQueue(
                station, member, &capturedQueue))
        {
            SetStationActionStatus(Localize(
                "status.station_action.queue_or_station_changed",
                "The member queue or station changed. No jobs were changed."));
            return;
        }

        g_pendingAction = PendingAction();
        g_pendingAction.type = ACTION_REMOVE_STATION_BUNDLE;
        g_pendingAction.member = member;
        g_pendingAction.stationTarget = station;
        g_pendingAction.sequence = capturedQueue;
        SetStationActionStatus(Localize(
            "status.station_action.validating_removal",
            "Validating all jobs for this station..."));
    }

    bool SameStationQueuePrefix(
        const std::vector<JobRowSnapshot>& before,
        const std::vector<JobRowSnapshot>& after)
    {
        if (after.size() < before.size())
        {
            return false;
        }
        std::vector<JobRowSnapshot> afterPrefix(
            after.begin(), after.begin() + before.size());
        return SameQueue(before, afterPrefix);
    }

    bool IsExpectedStationAssignmentSuffix(
        const std::vector<JobRowSnapshot>& before,
        const std::vector<JobRowSnapshot>& after,
        TaskType normalizedTask,
        bool automaticBundle,
        const HandleIdentity& station)
    {
        if (!SameStationQueuePrefix(before, after))
        {
            return false;
        }
        const size_t suffixCount = after.size() - before.size();
        if (automaticBundle)
        {
            if (suffixCount != 2)
            {
                return false;
            }
            const JobRowSnapshot& first = after[before.size()];
            const JobRowSnapshot& second = after[before.size() + 1];
            return SameStationJobKindAndTarget(
                    first, normalizedTask, station) &&
                SameStationJobKindAndTarget(
                    second, OPERATE_STORAGE, station);
        }
        if (suffixCount != 1)
        {
            return false;
        }
        return SameStationJobKindAndTarget(
            after[before.size()], normalizedTask, station);
    }

    bool TryPatchStationActionProjection(
        const MemberSnapshot& memberAfter,
        const HandleIdentity& station,
        bool* naturalBecameUnassignedOut)
    {
        if (naturalBecameUnassignedOut == NULL)
        {
            return false;
        }
        *naturalBecameUnassignedOut = false;
        if (!g_stationScan.started || !memberAfter.identity.valid ||
            !station.valid)
        {
            return false;
        }
        StationMemberSnapshot* projectedMember = NULL;
        for (size_t squadIndex = 0;
             squadIndex < g_stationScan.squads.size(); ++squadIndex)
        {
            StationSquadSnapshot& squad = g_stationScan.squads[squadIndex];
            for (size_t memberIndex = 0;
                 memberIndex < squad.members.size(); ++memberIndex)
            {
                StationMemberSnapshot& candidate = squad.members[memberIndex];
                if (SameHandleIdentity(
                        candidate.identity, memberAfter.identity))
                {
                    if (projectedMember != NULL)
                    {
                        return false;
                    }
                    projectedMember = &candidate;
                }
            }
        }
        StationTargetSnapshot* projectedStation = NULL;
        for (size_t stationIndex = 0;
             stationIndex < g_stationScan.stations.size(); ++stationIndex)
        {
            StationTargetSnapshot& candidate =
                g_stationScan.stations[stationIndex];
            if (SameHandleIdentity(candidate.identity, station))
            {
                if (projectedStation != NULL)
                {
                    return false;
                }
                projectedStation = &candidate;
            }
        }
        if (projectedMember == NULL || projectedStation == NULL)
        {
            return false;
        }

        CopyMemberQueueIntoStationProjection(memberAfter, projectedMember);
        JoinStationAssignments(g_stationScan.squads, projectedStation);
        *naturalBecameUnassignedOut =
            projectedStation->naturalResourceException &&
            projectedStation->assignments.empty();
        return true;
    }

    void FinishStationMemberAction(
        const MemberSnapshot& memberAfter,
        const HandleIdentity& station,
        const std::string& status,
        bool fullyVerified)
    {
        bool naturalBecameUnassigned = false;
        const bool projectionPatched = fullyVerified &&
            TryPatchStationActionProjection(
                memberAfter, station, &naturalBecameUnassigned);
        TryNotifyVanillaSelectionUI();
        RefreshSquadView(true);
        if (projectionPatched && !naturalBecameUnassigned)
        {
            // RefreshSquadView observes the verified queue change and marks
            // the station join dirty. The value projection above is already
            // exact, so clear that flag after the Squad refresh.
            g_stationAssignmentsDirty = false;
            // The Stations view integration treats equal source/destination
            // identities as one affected row for add/remove actions.
            g_stationProjectionRefreshSource = memberAfter.identity;
            g_stationProjectionRefreshDestination = memberAfter.identity;
            g_stationProjectionRefreshRequested = true;
        }
        else
        {
            // Partial, unexpected, or unreadable results must rebuild from
            // fresh queues. An unassigned natural target also needs a full
            // target rescan so it disappears from the station inventory.
            g_stationAssignmentsDirty = g_stationScan.started;
        }
        if (fullyVerified)
        {
            MarkStationDetailChange(station, memberAfter.identity);
        }
        SetStationActionStatus(status);
    }

    void FinishUnverifiedStationMemberAction(
        const HandleIdentity& station,
        const HandleIdentity& member,
        const std::string& status)
    {
        (void)station;
        (void)member;
        TryNotifyVanillaSelectionUI();
        RefreshSquadView(true);
        g_stationAssignmentsDirty = g_stationScan.started;
        SetStationActionStatus(status);
    }

    void ProcessStationAssignmentRequest(const PendingAction& action)
    {
        MemberSnapshot before;
        if (!TryBuildLiveMemberByIdentity(action.member, &before) ||
            !SameQueue(before.jobs, action.sequence))
        {
            FinishUnverifiedStationMemberAction(
                action.stationTarget, action.member,
                Localize(
                    "status.station.assign_stopped_queue",
                    "Station assignment stopped. Verified job changes: 0. The member queue changed or became unavailable."));
            return;
        }

        bool allowAssignedNaturalException = false;
        if (!TryGetAssignedNaturalAddAuthorization(
                action.stationTarget,
                &allowAssignedNaturalException))
        {
            FinishUnverifiedStationMemberAction(
                action.stationTarget, action.member,
                Localize(
                    "status.station.assign_stopped_projection",
                    "Station assignment stopped. Verified job changes: 0. The station projection became unavailable."));
            return;
        }

        // Reacquire the exact full queue again after projection
        // authorization and immediately before the native add leaf. This
        // keeps the pre-call fingerprint boundary as narrow as possible.
        MemberSnapshot immediatelyBeforeAdd;
        if (!TryBuildLiveMemberByIdentity(
                action.member, &immediatelyBeforeAdd) ||
            !SameQueue(immediatelyBeforeAdd.jobs, before.jobs))
        {
            FinishUnverifiedStationMemberAction(
                action.stationTarget, action.member,
                Localize(
                    "status.station.assign_stopped_race",
                    "Station assignment stopped. Verified job changes: 0. The member queue changed immediately before assignment."));
            return;
        }

        TaskType normalizedTask = NULL_TASK;
        bool automaticBundle = false;
        const StationAddJobResult addResult =
            TryResolveAndAddStationJobOnce(
                action.member, action.stationTarget, before.jobs.size(),
                allowAssignedNaturalException,
                &normalizedTask, &automaticBundle);
        if (addResult == STATION_ADD_INVALID_TARGET)
        {
            SetStationActionStatus(Localize(
                "status.station.invalid_target_or_member",
                "The member or exact station is unavailable or is no longer player-managed. No jobs were changed."));
            return;
        }
        if (addResult == STATION_ADD_UNSUPPORTED_TASK)
        {
            SetStationActionStatus(Localize(
                "status.station.unsupported_job",
                "This station does not expose a supported permanent job. No jobs were changed."));
            return;
        }
        if (addResult == STATION_ADD_QUEUE_FULL)
        {
            SetStationActionStatus(Localize(
                "status.station.queue_full",
                "The member queue is too full for this station job. No jobs were changed."));
            return;
        }

        MemberSnapshot after;
        if (!TryBuildLiveMemberByIdentity(action.member, &after))
        {
            FinishUnverifiedStationMemberAction(
                action.stationTarget, action.member,
                Localize(
                    "status.station.assign_unverified_queue",
                    "The station job was sent, but verified job changes: 0. The queue became unavailable. Review the member queue."));
            return;
        }
        const bool expectedSuffix = IsExpectedStationAssignmentSuffix(
                before.jobs, after.jobs, normalizedTask,
                automaticBundle, action.stationTarget);
        if (!expectedSuffix)
        {
            FinishStationMemberAction(
                after, action.stationTarget,
                Localize(
                    "status.station.assign_unexpected_queue",
                    "Kenshi changed the queue in an unexpected way. Verified station job changes: 0. Review the member queue before another change."),
                false);
            return;
        }

        if (addResult == STATION_ADD_FAULTED)
        {
            if (SameQueue(before.jobs, after.jobs))
            {
                SetStationActionStatus(Localize(
                    "status.station.job_rejected",
                    "Kenshi rejected the station job. No jobs were changed."));
            }
            else
            {
                FinishStationMemberAction(
                    after, action.stationTarget,
                    Localize(
                        "status.station.assign_error_verified",
                        "Kenshi reported an error after changing the queue. The exact station job change was verified."),
                    true);
            }
            return;
        }

        const size_t addedCount = after.jobs.size() - before.jobs.size();
        const std::string status = LocalizeFormat(
            "status.station.assign_success",
            "Assigned {member} to this station. Permanent jobs added: {count}.",
            "{member}", after.name,
            "{count}", IntegerString(addedCount));
        FinishStationMemberAction(
            after, action.stationTarget, status, true);
    }

    void ProcessStationBundleRemovalRequest(const PendingAction& action)
    {
        MemberSnapshot current;
        if (!TryBuildLiveMemberByIdentity(action.member, &current) ||
            !SameQueue(current.jobs, action.sequence))
        {
            FinishUnverifiedStationMemberAction(
                action.stationTarget, action.member,
                Localize(
                    "status.station.remove_stopped_queue",
                    "Station removal stopped. Verified job removals: 0. The member queue changed or became unavailable."));
            return;
        }

        if (!TryValidateStationActionTargetIdentity(action.stationTarget))
        {
            SetStationActionStatus(Localize(
                "status.station.remove_unavailable",
                "The station is unavailable or is no longer player-managed. No jobs were changed."));
            return;
        }

        std::vector<int> removalSlots;
        for (size_t index = 0; index < current.jobs.size(); ++index)
        {
            const JobRowSnapshot& row = current.jobs[index];
            if (IsStationDisplayJob(row.taskType) && row.hasTarget &&
                SameHandleIdentity(row.target, action.stationTarget))
            {
                removalSlots.push_back(static_cast<int>(index));
            }
        }

        size_t removedCount = 0;
        for (size_t removalIndex = removalSlots.size();
             removalIndex > 0; --removalIndex)
        {
            const int slot = removalSlots[removalIndex - 1];
            // Revalidate the live handle and player/natural ownership gate
            // immediately before every mutation in the station bundle.
            if (!TryValidateStationActionTargetIdentity(
                    action.stationTarget))
            {
                const std::string status = LocalizeFormat(
                    "status.station.remove_stopped_target",
                    "Station removal stopped. Verified job removals: {count}. The exact station is no longer player-managed.",
                    "{count}", IntegerString(removedCount));
                FinishUnverifiedStationMemberAction(
                    action.stationTarget, action.member, status);
                return;
            }

            // Reacquire and compare the exact full queue after the target
            // validation and immediately before the native removal. This
            // catches external or engine-side changes between bundle rows.
            MemberSnapshot beforeRemove;
            if (!TryBuildLiveMemberByIdentity(
                    action.member, &beforeRemove) ||
                !SameQueue(beforeRemove.jobs, current.jobs) ||
                slot < 0 ||
                slot >= static_cast<int>(beforeRemove.jobs.size()) ||
                !IsStationDisplayJob(beforeRemove.jobs[slot].taskType) ||
                !beforeRemove.jobs[slot].hasTarget ||
                !SameHandleIdentity(
                    beforeRemove.jobs[slot].target,
                    action.stationTarget))
            {
                const std::string status = LocalizeFormat(
                    "status.station.remove_stopped_queue_changed",
                    "Station removal stopped. Verified job removals: {count}. The exact full queue changed or became unavailable.",
                    "{count}", IntegerString(removedCount));
                FinishUnverifiedStationMemberAction(
                    action.stationTarget, action.member, status);
                return;
            }
            const bool removeCallSucceeded =
                TryRemovePermajob(beforeRemove.handle, slot);

            MemberSnapshot afterRemove;
            if (!TryBuildLiveMemberByIdentity(action.member, &afterRemove))
            {
                const std::string status = LocalizeFormat(
                    "status.station.remove_stopped_unavailable",
                    "Station removal stopped. Verified job removals: {count}. The changed queue became unavailable. Review it before another change.",
                    "{count}", IntegerString(removedCount));
                FinishUnverifiedStationMemberAction(
                    action.stationTarget, action.member, status);
                return;
            }

            const bool exactRemoval = SameQueueWithExactJobRemoved(
                beforeRemove.jobs, slot, afterRemove.jobs);
            current = afterRemove;
            if (exactRemoval)
            {
                ++removedCount;
            }
            if (!removeCallSucceeded || !exactRemoval)
            {
                const std::string status = !exactRemoval ?
                    LocalizeFormat(
                        "status.station.remove_stopped_unexpected",
                        "Station removal stopped. Verified job removals: {count}. The queue changed in an unexpected way. Review it before another change.",
                        "{count}", IntegerString(removedCount)) :
                    LocalizeFormat(
                        "status.station.remove_stopped_error",
                        "Station removal stopped. Verified job removals: {count}. Kenshi reported an error after the removal. Review it before another change.",
                        "{count}", IntegerString(removedCount));
                FinishStationMemberAction(
                    current, action.stationTarget, status, false);
                return;
            }
        }

        if (removedCount == 0)
        {
            FinishStationMemberAction(
                current, action.stationTarget,
                LocalizeFormat(
                    "status.station.remove_none",
                    "{member} has no permanent jobs for this station.",
                    "{member}", current.name),
                true);
        }
        else
        {
            FinishStationMemberAction(
                current, action.stationTarget,
                LocalizeFormat(
                    "status.station.remove_success",
                    "Removed permanent station jobs: {count}. Member: {member}.",
                    "{count}", IntegerString(removedCount),
                    "{member}", current.name),
                true);
        }
    }

    void FinishStationTransfer(const std::string& status)
    {
        g_stationAssignmentsDirty = g_stationScan.started;
        TryNotifyVanillaSelectionUI();
        RefreshSquadView(true);
        SetStatus(status);
    }

    void FinishSuccessfulStationTransfer(
        const std::string& status,
        const MemberSnapshot& sourceAfter,
        const MemberSnapshot& destinationAfter)
    {
        const bool projectionPatched =
            TryPatchStationTransferProjection(
                &g_stationScan, sourceAfter, destinationAfter);
        TryNotifyVanillaSelectionUI();
        RefreshSquadView(true);
        if (projectionPatched)
        {
            // Refresh only the visible station widgets from the patched
            // snapshot.  Do not discard columns, scan progress, filters,
            // collapsed squads, selection, or scroll positions.
            g_stationAssignmentsDirty = false;
            g_stationProjectionRefreshSource = sourceAfter.identity;
            g_stationProjectionRefreshDestination = destinationAfter.identity;
            g_stationProjectionRefreshRequested = true;
        }
        else
        {
            // If the in-memory projection no longer contains both members,
            // fall back to the normal fail-closed station projection rebuild.
            g_stationAssignmentsDirty = g_stationScan.started;
        }
        SetStatus(status);
    }

    void ProcessStationJobTransfer(const PendingAction& action)
    {
        if (!action.job.hasTarget || !action.job.target.valid ||
            !SameHandleIdentity(action.job.target, action.stationTarget) ||
            action.stationTarget.type != BUILDING)
        {
            SetStatus(Localize(
                "status.station.transfer_target_unavailable",
                "The station target is unavailable. No jobs were changed."));
            return;
        }
        if (SameHandleIdentity(action.member, action.destinationMember))
        {
            SetStatus(Localize(
                "status.station.transfer_same_member",
                "Choose a different member. No jobs were changed."));
            return;
        }

        MemberSnapshot source;
        MemberSnapshot destination;
        if (!TryBuildLiveMemberByIdentity(action.member, &source) ||
            !SameQueue(source.jobs, action.sequence))
        {
            g_stationAssignmentsDirty = g_stationScan.started;
            SetStatus(Localize(
                "status.station.transfer_source_changed",
                "The source queue changed or became unavailable. No jobs were changed."));
            return;
        }
        if (!TryBuildLiveMemberByIdentity(
                action.destinationMember, &destination) ||
            !SameQueue(destination.jobs, action.destinationSequence))
        {
            g_stationAssignmentsDirty = g_stationScan.started;
            SetStatus(Localize(
                "status.station.transfer_destination_changed",
                "The destination queue changed or became unavailable. No jobs were changed."));
            return;
        }
        if (destination.jobs.size() >=
            static_cast<size_t>(MAX_SAFE_JOB_ROWS))
        {
            g_stationAssignmentsDirty = g_stationScan.started;
            SetStatus(Localize(
                "status.station.transfer_destination_full",
                "The destination queue is at the 64-job safety limit. No jobs were changed."));
            return;
        }

        const int sourceSlot = FindJobSlot(source, action.job);
        if (sourceSlot < 0 ||
            HasStationJobKindAndTarget(
                destination.jobs, action.job.taskType, action.stationTarget))
        {
            g_stationAssignmentsDirty = g_stationScan.started;
            SetStatus(sourceSlot < 0 ?
                Localize(
                    "status.station.transfer_source_missing",
                    "The exact source job is no longer available. No jobs were changed.") :
                Localize(
                    "status.station.transfer_duplicate",
                    "That member already has the same job for this station. No jobs were changed."));
            return;
        }

        if (!TryAddStationPermanentJob(
                destination.handle, action.job.taskType,
                action.stationTarget))
        {
            g_stationAssignmentsDirty = g_stationScan.started;
            SetStatus(Localize(
                "status.station.transfer_destination_rejected",
                "Kenshi rejected the destination job. No jobs were changed."));
            return;
        }

        MemberSnapshot destinationAfter;
        if (!TryBuildLiveMemberByIdentity(
                action.destinationMember, &destinationAfter) ||
            !SameQueueWithAppendedStationJob(
                destination.jobs, destinationAfter.jobs,
                action.job.taskType, action.stationTarget))
        {
            FinishStationTransfer(
                Localize(
                    "status.station.transfer_destination_unverified",
                    "The destination could not be verified after adding the job. The source was not changed; review both queues."));
            return;
        }

        // Recheck the source independently after the destination mutation.
        // Never remove a row if its full queue fingerprint changed.
        MemberSnapshot sourceBeforeRemove;
        if (!TryBuildLiveMemberByIdentity(action.member, &sourceBeforeRemove) ||
            !SameQueue(sourceBeforeRemove.jobs, action.sequence))
        {
            FinishStationTransfer(
                Localize(
                    "status.station.transfer_source_changed_after_add",
                    "The destination job was added, but the source queue changed. The source was not removed; a duplicate may remain."));
            return;
        }
        const int verifiedSourceSlot =
            FindJobSlot(sourceBeforeRemove, action.job);
        if (verifiedSourceSlot < 0 ||
            !TryRemovePermajob(sourceBeforeRemove.handle, verifiedSourceSlot))
        {
            FinishStationTransfer(
                Localize(
                    "status.station.transfer_source_remove_rejected",
                    "The destination job was added, but Kenshi rejected the source removal. A duplicate remains; review both queues."));
            return;
        }

        MemberSnapshot sourceAfter;
        if (!TryBuildLiveMemberByIdentity(action.member, &sourceAfter) ||
            !SameQueueWithExactJobRemoved(
                sourceBeforeRemove.jobs, verifiedSourceSlot,
                sourceAfter.jobs))
        {
            FinishStationTransfer(
                Localize(
                    "status.station.transfer_source_remove_unverified",
                    "The destination job was added, but the source removal could not be verified. Review both queues before making another change."));
            return;
        }

        // The exact job label is stable even if the station column is filtered
        // before the next snapshot refresh.
        const std::string status = LocalizeFormat(
            "status.station.transfer_success",
            "Moved {job} from {source} to {destination}.",
            "{job}", StripLeadingPriorityPrefix(action.job.jobLabel),
            "{source}", source.name,
            "{destination}", destination.name);
        FinishSuccessfulStationTransfer(
            status, sourceAfter, destinationAfter);
    }

    void ProcessPendingAction()
    {
        if (g_pendingAction.type == ACTION_NONE)
        {
            return;
        }
        PendingAction action = g_pendingAction;
        g_pendingAction = PendingAction();

        if (action.type == ACTION_REMOVE_SELECTED)
        {
            ProcessRemoveSelected();
            return;
        }

        if (action.type == ACTION_TRANSFER_STATION_JOB)
        {
            ProcessStationJobTransfer(action);
            return;
        }

        if (action.type == ACTION_ASSIGN_STATION)
        {
            ProcessStationAssignmentRequest(action);
            return;
        }

        if (action.type == ACTION_REMOVE_STATION_BUNDLE)
        {
            ProcessStationBundleRemovalRequest(action);
            return;
        }

        MemberSnapshot fresh;
        if (!TryBuildFreshRosterMemberByIdentity(action.member, &fresh))
        {
            RefreshSquadView(true);
            SetStatus(Localize(
                "status.member.queue_unavailable",
                "The member queue is unavailable. No jobs were changed."));
            return;
        }

        if (action.type == ACTION_TOGGLE_JOBS)
        {
            if (!TrySetJobsEnabled(fresh.handle, !fresh.jobsEnabled))
            {
                SetStatus(Localize(
                    "status.jobs.toggle_rejected",
                    "Kenshi rejected the Jobs toggle. No jobs were changed."));
                return;
            }
            TryNotifyVanillaSelectionUI();
            RefreshSquadView(true);
            SetStatus(fresh.jobsEnabled ? Localize(
                "status.jobs.disabled", "Jobs disabled for this member.") :
                Localize(
                    "status.jobs.enabled", "Jobs enabled for this member."));
            return;
        }

        if (action.type == ACTION_REORDER)
        {
            if (!SameQueue(fresh.jobs, action.sequence))
            {
                RefreshSquadView(true);
                SetStatus(Localize(
                    "status.drag.queue_changed",
                    "The queue changed during the drag. Review it and try again."));
                return;
            }
            const int source = FindJobSlot(fresh, action.job);
            if (source < 0 || action.targetSlot < 0 ||
                action.targetSlot >= static_cast<int>(fresh.jobs.size()) ||
                !TryMovePermajob(fresh.handle, source, action.targetSlot))
            {
                RefreshSquadView(true);
                SetStatus(Localize(
                    "status.jobs.reorder_rejected",
                    "Kenshi rejected the queue reorder. No other jobs were changed."));
                return;
            }
            TryNotifyVanillaSelectionUI();
            RefreshSquadView(true);
            SetStatus(Localize(
                "status.jobs.priority_updated",
                "Permanent-job priority updated."));
            return;
        }

        if (action.type == ACTION_CLEAR_MEMBER)
        {
            if (!SameQueue(fresh.jobs, action.sequence))
            {
                RefreshSquadView(true);
                SetStatus(Localize(
                    "status.clear.queue_changed",
                    "The queue changed after confirmation. Clear Queue was cancelled."));
                return;
            }
            if (!TryClearPermajobs(fresh.handle))
            {
                SetStatus(Localize(
                    "status.clear.rejected",
                    "Kenshi rejected Clear Queue. No jobs were changed."));
                return;
            }
            for (size_t index = g_selectedJobs.size(); index > 0; --index)
            {
                if (SameHandleIdentity(
                        g_selectedJobs[index - 1].member,
                        action.member))
                {
                    g_selectedJobs.erase(g_selectedJobs.begin() + index - 1);
                }
            }
            SyncRecipientsToSelectedJobs();
            TryNotifyVanillaSelectionUI();
            RefreshSquadView(true);
            SetStatus(LocalizeFormat(
                "status.clear.success",
                "All permanent jobs were removed from {member}.",
                "{member}", fresh.name));
        }
    }

    void TickDragAutoScroll(DWORD now)
    {
        if (!g_drag.active || g_jobViewport == NULL ||
            !HasElapsed(now, g_lastDragTick, 16))
        {
            return;
        }
        const DWORD elapsed = g_lastDragTick == 0 ? 16 : now - g_lastDragTick;
        g_lastDragTick = now;
        const MyGUI::IntPoint mouse =
            MyGUI::InputManager::getInstance().getMousePosition();
        const MyGUI::IntCoord view = g_jobViewport->getAbsoluteCoord();
        int horizontalDelta = 0;
        if (mouse.left >= view.left && mouse.left < view.left + DRAG_EDGE)
        {
            horizontalDelta = -static_cast<int>(600.0f * elapsed / 1000.0f);
        }
        else if (mouse.left < view.right() && mouse.left >= view.right() - DRAG_EDGE)
        {
            horizontalDelta = static_cast<int>(600.0f * elapsed / 1000.0f);
        }
        int verticalDelta = 0;
        if (mouse.top >= view.top && mouse.top < view.top + DRAG_EDGE)
        {
            verticalDelta = -static_cast<int>(600.0f * elapsed / 1000.0f);
        }
        else if (mouse.top < view.bottom() &&
                 mouse.top >= view.bottom() - DRAG_EDGE)
        {
            verticalDelta = static_cast<int>(600.0f * elapsed / 1000.0f);
        }
        if (horizontalDelta == 0 && verticalDelta == 0)
        {
            UpdateDragInsertion(mouse);
            return;
        }

        const int nextHorizontal = ClampInt(
            g_horizontalOffset + horizontalDelta,
            0,
            g_maxHorizontalOffset);
        const int nextVertical = ClampInt(
            g_verticalOffset + verticalDelta,
            0,
            g_maxVerticalOffset);
        if (nextHorizontal != g_horizontalOffset ||
            nextVertical != g_verticalOffset)
        {
            g_horizontalOffset = nextHorizontal;
            g_verticalOffset = nextVertical;
            g_changingScroll = true;
            if (g_horizontalScroll != NULL)
            {
                SetThemeScrollBarPosition(
                    g_horizontalScroll,
                    static_cast<size_t>(g_horizontalOffset));
            }
            if (g_verticalScroll != NULL)
            {
                SetThemeScrollBarPosition(
                    g_verticalScroll,
                    static_cast<size_t>(g_verticalOffset));
            }
            g_changingScroll = false;
            ApplyScrollOffsets();
            UpdateDragInsertion(mouse);
        }
    }
