/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef TIMELINEMODEL_H
#define TIMELINEMODEL_H

#include "undohelper.hpp"
#include <QAbstractItemModel>
#include <QReadWriteLock>
#include <memory>
#include <mlt++/MltTractor.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define LOGGING 1 // If set to 1, we log the actions requested to the timeline as a reproducer script
#ifdef LOGGING
#include <fstream>
#endif

class AssetParameterModel;
class EffectStackModel;
class ClipModel;
class CompositionModel;
class DocUndoStack;
class GroupsModel;
class SnapModel;
class TimelineItemModel;
class TrackModel;

/* @brief This class represents a Timeline object, as viewed by the backend.
   In general, the Gui associated with it will send modification queries (such as resize or move), and this class authorize them or not depending on the
   validity of the modifications.

   This class also serves to keep track of all objects. It holds pointers to all tracks and clips, and gives them unique IDs on creation. These Ids are used in
   any interactions with the objects and have nothing to do with Melt IDs.

   This is the entry point for any modifications that has to be made on an element. The dataflow beyond this entry point may vary, for example when the user
   request a clip resize, the call is deferred to the clip itself, that check if there is enough data to extend by the requested amount, compute the new in and
   out, and then asks the track if there is enough room for extension. To avoid any confusion on which function to call first, rembember to always call the
   version in timeline. This is also required to generate the Undo/Redo operators

   Generally speaking, we don't check ahead of time if an action is going to succeed or not before applying it.
   We just apply it naively, and if it fails at some point, we use the undo operator that we are constructing on the fly to revert what we have done so far.
   For example, when we move a group of clips, we apply the move operation to all the clips inside this group (in the right order). If none fails, we are good,
   otherwise we revert what we've already done.
   This kind of behaviour frees us from the burden of simulating the actions before actually applying theme. This is a good thing because this simulation step
   would be very sensitive to corruptions and small discrepancies, which we try to avoid at all cost.


   It derives from AbstractItemModel (indirectly through TimelineItemModel) to provide the model to the QML interface. An itemModel is organized with row and
   columns that contain the data. It can be hierarchical, meaning that a given index (row,column) can contain another level of rows and column.
   Our organization is as follows: at the top level, each row contains a track. These rows are in the same order as in the actual timeline.
   Then each of this row contains itself sub-rows that correspond to the clips.
   Here the order of these sub-rows is unrelated to the chronological order of the clips,
   but correspond to their Id order. For example, if you have three clips, with ids 12, 45 and 150, they will receive row index 0,1 and 2.
   This is because the order actually doesn't matter since the clips are rendered based on their positions rather than their row order.
   The id order has been choosed because it is consistant with a valid ordering of the clips.
   The columns are never used, so the data is always in column 0

   An ModelIndex in the ItemModel consists of a row number, a column number, and a parent index. In our case, tracks have always an empty parent, and the clip
   have a track index as parent.
   A ModelIndex can also store one additional integer, and we exploit this feature to store the unique ID of the object it corresponds to.

*/
class TimelineModel : public QAbstractItemModel, public std::enable_shared_from_this<TimelineModel>
{
    Q_OBJECT

protected:
    /* @brief this constructor should not be called. Call the static construct instead
     */
    TimelineModel(Mlt::Profile *profile, std::weak_ptr<DocUndoStack> undo_stack);

public:
    friend class TrackModel;
    template <typename T> friend class MoveableItem;
    friend class ClipModel;
    friend class CompositionModel;
    friend class GroupsModel;

    /// Two level model: tracks and clips on track
    enum {
        NameRole = Qt::UserRole + 1,
        ResourceRole, /// clip only
        ServiceRole,  /// clip only
        IsBlankRole,  /// clip only
        StartRole,    /// clip only
        BinIdRole,    /// clip only
        MarkersRole,  /// clip only
        DurationRole,
        InPointRole,   /// clip only
        OutPointRole,  /// clip only
        FramerateRole, /// clip only
        GroupedRole,   /// clip only
        HasAudio,      /// clip only
        IsMuteRole,    /// track only
        IsHiddenRole,  /// track only
        IsAudioRole,
        AudioLevelsRole,   /// clip only
        IsCompositeRole,   /// track only
        IsLockedRole,      /// track only
        HeightRole,        /// track only
        FadeInRole,        /// clip only
        FadeOutRole,       /// clip only
        IsCompositionRole, /// clip only
        FileHashRole,      /// clip only
        SpeedRole,         /// clip only
        ItemATrack,        /// composition only
        ItemIdRole
    };

    virtual ~TimelineModel();
    Mlt::Tractor *tractor() const { return m_tractor.get(); }
    /* @brief Load tracks from the current tractor, used on project opening
     */
    void loadTractor();

    /* @brief Returns the current tractor's producer, useful fo control seeking, playing, etc
     */
    Mlt::Producer *producer();
    Mlt::Profile *getProfile();

    /* @brief returns the number of tracks */
    int getTracksCount() const;

    /* @brief returns the number of clips */
    int getClipsCount() const;

    /* @brief returns the number of compositions */
    int getCompositionsCount() const;

    /* @brief Returns the id of the track containing clip (-1 if it is not inserted)
       @param clipId Id of the clip to test */
    Q_INVOKABLE int getClipTrackId(int clipId) const;

    /* @brief Returns the id of the track containing composition (-1 if it is not inserted)
       @param clipId Id of the composition to test */
    Q_INVOKABLE int getCompositionTrackId(int compoId) const;

    /* @brief Convenience function that calls either of the previous ones based on item type*/
    int getItemTrackId(int itemId) const;

    /* @brief Helper function that returns true if the given ID corresponds to a clip */
    bool isClip(int id) const;

    /* @brief Helper function that returns true if the given ID corresponds to a composition */
    bool isComposition(int id) const;

    /* @brief Helper function that returns true if the given ID corresponds to a track */
    bool isTrack(int id) const;

    /* @brief Helper function that returns true if the given ID corresponds to a track */
    bool isGroup(int id) const;

    /* @brief Given a composition Id, returns its underlying parameter model */
    std::shared_ptr<AssetParameterModel> getCompositionParameterModel(int compoId) const;
    /* @brief Given a clip Id, returns its underlying effect stack model */
    std::shared_ptr<EffectStackModel> getClipEffectStackModel(int clipId) const;

    /* @brief Returns the position of clip (-1 if it is not inserted)
       @param clipId Id of the clip to test
    */
    Q_INVOKABLE int getClipPosition(int clipId) const;
    Q_INVOKABLE bool addClipEffect(int clipId, const QString &effectId);
    Q_INVOKABLE bool copyClipEffect(int clipId, const QString &sourceId, const QString &rowId);

    /* @brief Returns the closest snap point within snapDistance
     */
    Q_INVOKABLE int suggestSnapPoint(int pos, int snapDistance);

    /* @brief Returns the in cut position of a clip
       @param clipId Id of the clip to test
    */
    int getClipIn(int clipId) const;

    /* @brief Returns the bin id of the clip master
       @param clipId Id of the clip to test
    */
    const QString getClipBinId(int clipId) const;

    /* @brief Returns the duration of a clip
       @param clipId Id of the clip to test
    */
    int getClipPlaytime(int clipId) const;

    /* @brief Returns the number of clips in a given track
       @param trackId Id of the track to test
    */
    int getTrackClipsCount(int trackId) const;

    /* @brief Returns the number of compositions in a given track
       @param trackId Id of the track to test
    */
    int getTrackCompositionsCount(int trackId) const;

    /* @brief Returns the position of the track in the order of the tracks
       @param trackId Id of the track to test
    */
    int getTrackPosition(int trackId) const;

    /* @brief Returns the track's index in terms of mlt's internal representation
     */
    int getTrackMltIndex(int trackId) const;

    /* @brief Returns the id of the track just below the given track in the order of the tracks
       Return -1 if we give the last track
       @param trackId Id of the track to test
    */
    int getNextTrackId(int trackId) const;

    /* @brief Returns the id of the track just above the given track in the order of the tracks
       Return -1 if we give the last track
       @param trackId Id of the track to test
    */
    int getPreviousVideoTrackId(int trackId) const;

    /* @brief Move a clip to a specific position
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       If the clip is not in inserted in a track yet, it gets inserted for the first time.
       If the clip is in a group, the call is deferred to requestGroupMove
       @param clipId is the ID of the clip
       @param trackId is the ID of the target track
       @param position is the position where we want to move
       @param updateView if set to false, no signal is sent to qml
       @param logUndo if set to false, no undo object is stored
    */
    Q_INVOKABLE bool requestClipMove(int clipId, int trackId, int position, bool updateView = true, bool logUndo = true);

    /* @brief Move a composition to a specific position This action is undoable
       Returns true on success. If it fails, nothing is modified. If the clip is
       not in inserted in a track yet, it gets inserted for the first time. If
       the clip is in a group, the call is deferred to requestGroupMove @param
       transid is the ID of the composition @param trackId is the ID of the
       track */
    Q_INVOKABLE bool requestCompositionMove(int compoId, int trackId, int position, bool updateView = true, bool logUndo = true);

    /* Same function, but accumulates undo and redo, and doesn't check
       for group*/
    bool requestClipMove(int clipId, int trackId, int position, bool updateView, Fun &undo, Fun &redo);
    bool requestCompositionMove(int transid, int trackId, int position, bool updateView, Fun &undo, Fun &redo);

    Q_INVOKABLE int getCompositionPosition(int compoId) const;
    Q_INVOKABLE int suggestCompositionMove(int compoId, int trackId, int position, int snapDistance = -1);
    int getCompositionPlaytime(int compoId) const;

    /* Returns an item position, item can be clip or composition */
    int getItemPosition(int itemId) const;
    /* Returns an item duration, item can be clip or composition */
    int getItemPlaytime(int itemId) const;

    /* @brief Given an intended move, try to suggest a more valid one
       (accounting for snaps and missing UI calls) 
       @param clipId id of the clip to
       move 
       @param trackId id of the target track 
       @param position target position
       @param snapDistance the maximum distance for a snap result, -1 for no snapping
        of the clip */
    Q_INVOKABLE int suggestClipMove(int clipId, int trackId, int position, int snapDistance = -1);

    /* @brief Request clip insertion at given position. This action is undoable
       Returns true on success. If it fails, nothing is modified. @param
       binClipId id of the clip in the bin @param track Id of the track where to
       insert @param Requested position @param ID return parameter of the id of
       the inserted clip @param logUndo if set to false, no undo object is
       stored */ bool
    requestClipInsertion(const QString &binClipId, int trackId, int position, int &id, bool logUndo = true); /* Same function,
                                                       but accumulates undo and redo*/
    bool requestClipInsertion(const QString &binClipId, int trackId, int position, int &id, Fun &undo, Fun &redo);

    /* @brief Deletes the given clip or composition from the timeline This
       action is undoable Returns true on success. If it fails, nothing is
       modified. If the clip/composition is in a group, the call is deferred to
       requestGroupDeletion @param clipId is the ID of the clip/composition
       @param logUndo if set to false, no undo object is stored */
    Q_INVOKABLE bool requestItemDeletion(int clipId, bool logUndo = true);
    /* Same function, but accumulates undo and redo, and doesn't check for group*/
    bool requestClipDeletion(int clipId, Fun &undo, Fun &redo);
    bool requestCompositionDeletion(int compositionId, Fun &undo, Fun &redo);

    /* @brief Move a group to a specific position
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       If the clips in the group are not in inserted in a track yet, they get inserted for the first time.
       @param clipId is the id of the clip that triggers the group move
       @param groupId is the id of the group
       @param delta_track is the delta applied to the track index
       @param delta_pos is the requested position change
       @param updateView if set to false, no signal is sent to qml for the clip clipId
       @param logUndo if set to true, an undo object is created
    */
    bool requestGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView = true, bool logUndo = true);
    bool requestGroupMove(int clipId, int groupId, int delta_track, int delta_pos, bool updateView, Fun &undo, Fun &redo);

    /* @brief Deletes all clips inside the group that contains the given clip.
       This action is undoable
       Note that if their is a hierarchy of groups, all of them will be deleted.
       Returns true on success. If it fails, nothing is modified.
       @param clipId is the id of the clip that triggers the group deletion
    */
    Q_INVOKABLE bool requestGroupDeletion(int clipId, bool logUndo = true);

    /* @brief Change the duration of an item (clip or composition)
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param itemId is the ID of the item
       @param size is the new size of the item
       @param right is true if we change the right side of the item, false otherwise
       @param logUndo if set to true, an undo object is created
       @param snap if set to true, the resize order will be coerced to use the snapping grid
    */
    Q_INVOKABLE bool requestItemResize(int itemId, int size, bool right, bool logUndo = true, int snapDistance = -1);
    /* @brief Change the duration of an item (clip or composition)
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param itemId is the ID of the item
       @param position is the requested start or end position
       @param resizeStart is true if we want to resize clip start, false to resize clip end
    */
    bool requestItemResizeToPos(int itemId, int position, bool right);

    /* Same function, but accumulates undo and redo and doesn't deal with snapping*/
    bool requestItemResize(int itemId, int size, bool right, bool logUndo, Fun &undo, Fun &redo, bool blockUndo = false);

    /* @brief Similar to requestClipResize but takes a delta instead of absolute size
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param clipId is the ID of the clip
       @param delta is the delta to be applied to the length of the clip
       @param right is true if we change the right side of the clip, false otherwise
       @param ripple TODO document this
       @param test_only if set to true, the undo is not created and no signal is sent to qml
     */
    bool requestClipTrim(int clipId, int delta, bool right, bool ripple = false, bool test_only = false);

    /* @brief Group together a set of ids
       The ids are either a group ids or clip ids. The involved clip must already be inserted in a track
       This action is undoable
       Returns the group id on success, -1 if it fails and nothing is modified.
       Typically, ids would be ids of clips, but for convenience, some of them can be ids of groups as well.
       @param ids Set of ids to group
    */
    int requestClipsGroup(const std::unordered_set<int> &ids, bool logUndo = true);
    int requestClipsGroup(const std::unordered_set<int> &ids, Fun &undo, Fun &redo);

    /* @brief Destruct the topmost group containing clip
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param id of the clip to degroup (all clips belonging to the same group will be ungrouped as well)
    */
    bool requestClipUngroup(int id, bool logUndo = true);
    /* Same function, but accumulates undo and redo*/
    bool requestClipUngroup(int id, Fun &undo, Fun &redo);

    /* @brief Create a track at given position
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param Requested position (order). If set to -1, the track is inserted last.
       @param id is a return parameter that holds the id of the resulting track (-1 on failure)
    */
    bool requestTrackInsertion(int pos, int &id);
    /* Same function, but accumulates undo and redo*/
    bool requestTrackInsertion(int pos, int &id, Fun &undo, Fun &redo);

    /* @brief Delete track with given id
       This also deletes all the clips contained in the track.
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param trackId id of the track to delete
    */
    bool requestTrackDeletion(int trackId);
    /* Same function, but accumulates undo and redo*/
    bool requestTrackDeletion(int trackId, Fun &undo, Fun &redo);

    /* @brief Get project duration
       Returns the duration in frames
    */
    int duration() const;

    /* @brief Get all the elements of the same group as the given clip.
       If there is a group hierarchy, only the topmost group is considered.
       @param clipId id of the clip to test
    */
    std::unordered_set<int> getGroupElements(int clipId);

    /* @brief Removes all the elements on the timeline (tracks and clips)
     */
    bool requestReset(Fun &undo, Fun &redo);
    /* @brief Updates the current the pointer to the current undo_stack
       Must be called for example when the doc change
    */
    void setUndoStack(std::weak_ptr<DocUndoStack> undo_stack);

    /* @brief Requests the best snapped position for a clip
       @param pos is the clip's requested position
       @param length is the clip's duration
       @param pts snap points to ignore (for example currently moved clip)
       @param snapDistance the maximum distance for a snap result, -1 for no snapping
       @returns best snap position or -1 if no snap point is near
     */
    int requestBestSnapPos(int pos, int length, const std::vector<int> &pts = std::vector<int>(), int snapDistance = -1);

    /* @brief Requests the next snapped point
       @param pos is the current position
     */
    int requestNextSnapPos(int pos);

    /* @brief Requests the previous snapped point
       @param pos is the current position
     */
    int requestPreviousSnapPos(int pos);

    /* @brief Request composition insertion at given position.
       This action is undoable
       Returns true on success. If it fails, nothing is modified.
       @param transitionId Identifier of the Mlt transition to insert (as given by repository)
       @param track Id of the track where to insert
       @param position Requested position
       @param length Requested initial length.
       @param id return parameter of the id of the inserted composition
       @param logUndo if set to false, no undo object is stored
    */
    bool requestCompositionInsertion(const QString &transitionId, int trackId, int position, int length, int &id, bool logUndo = true);
    /* Same function, but accumulates undo and redo*/
    bool requestCompositionInsertion(const QString &transitionId, int trackId, int position, int length, int &id, Fun &undo, Fun &redo);

    /* @brief This function change the global (timeline-wise) enabled state of the effects
       It disables/enables track and clip effects (recursively)
     */
    void setTimelineEffectsEnabled(bool enabled);

    /* @brief Get a timeline clip id by its position
     */
    int getClipByPosition(int trackId, int position) const;

    /* @brief Creates a new clip instance without inserting it
     */
    bool requestClipCreation(const QString &binClipId, int in, int duration, int &id, Fun &undo, Fun &redo);

    /* @brief Returns a list of all items that are at or after a given position.
     * @param trackId is the id of the track for concerned items. Setting trackId to -1 returns items on all tracks
     * @param position is the position where we the items should start
     * @param end is the position after which items will not be selected, set to -1 to get all clips on track
     * @param listCompositions if enabled, the list will also contains composition ids
     */
    std::unordered_set<int> getItemsAfterPosition(int trackId, int position, int end = -1, bool listCompositions = true);

    /* @brief Returns a list of all luma files used in the project
     */
    QStringList extractCompositionLumas() const;

protected:
    /* @brief Register a new track. This is a call-back meant to be called from TrackModel
       @param pos indicates the number of the track we are adding. If this is -1, then we add at the end.
     */
    void registerTrack(std::shared_ptr<TrackModel> track, int pos = -1, bool doInsert = true);

    /* @brief Register a new clip. This is a call-back meant to be called from ClipModel
    */
    void registerClip(const std::shared_ptr<ClipModel> &clip);

    /* @brief Register a new composition. This is a call-back meant to be called from CompositionModel
    */
    void registerComposition(const std::shared_ptr<CompositionModel> &composition);

    /* @brief Register a new group. This is a call-back meant to be called from GroupsModel
     */
    void registerGroup(int groupId);

    /* @brief Deregister and destruct the track with given id.
       @parame updateView Whether to send updates to the model. Must be false when called from a constructor/destructor
     */
    Fun deregisterTrack_lambda(int id, bool updateView = false);

    /* @brief Return a lambda that deregisters and destructs the clip with given id.
       Note that the clip must already be deleted from its track and groups.
     */
    Fun deregisterClip_lambda(int id);

    /* @brief Return a lambda that deregisters and destructs the composition with given id.
     */
    Fun deregisterComposition_lambda(int compoId);

    /* @brief Deregister a group with given id
     */
    void deregisterGroup(int id);

    /* @brief Helper function to get a pointer to the track, given its id
     */
    std::shared_ptr<TrackModel> getTrackById(int trackId);
    const std::shared_ptr<TrackModel> getTrackById_const(int trackId) const;

    /*@brief Helper function to get a pointer to a clip, given its id*/
    std::shared_ptr<ClipModel> getClipPtr(int clipId) const;

    /*@brief Helper function to get a pointer to a composition, given its id*/
    std::shared_ptr<CompositionModel> getCompositionPtr(int compoId) const;

    /* @brief Returns next valid unique id to create an object
     */
    static int getNextId();

    /* @brief unplant and the replant all the compositions in the correct order
       @param currentCompo is the id of a compo that have not yet been planted, if any. Otherwise send -1
     */
    bool replantCompositions(int currentCompo);

    /* @brief Unplant the composition with given Id */
    bool unplantComposition(int compoId);

    /* @brief Debugging function that checks consistency with Mlt objects */
    bool checkConsistency();

    void checkRefresh(int start, int end) const;

protected:
    std::unique_ptr<Mlt::Tractor> m_tractor;

    std::list<std::shared_ptr<TrackModel>> m_allTracks;

    std::unordered_map<int, std::list<std::shared_ptr<TrackModel>>::iterator>
        m_iteratorTable; // this logs the iterator associated which each track id. This allows easy access of a track based on its id.

    std::unordered_map<int, std::shared_ptr<ClipModel>> m_allClips; // the keys are the clip id, and the values are the corresponding pointers

    std::unordered_map<int, std::shared_ptr<CompositionModel>>
        m_allCompositions; // the keys are the composition id, and the values are the corresponding pointers

    static int next_id; // next valid id to assign

    std::unique_ptr<GroupsModel> m_groups;
    std::shared_ptr<SnapModel> m_snaps;

    std::unordered_set<int> m_allGroups; // ids of all the groups

    std::weak_ptr<DocUndoStack> m_undoStack;

    Mlt::Profile *m_profile;

    // The black track producer. Its length / out should always be adjusted to the projects's length
    std::unique_ptr<Mlt::Producer> m_blackClip;

    mutable QReadWriteLock m_lock; // This is a lock that ensures safety in case of concurrent access

    std::ofstream m_logFile; // this is a temporary debug member to help reproduce issues

    bool m_timelineEffectsEnabled;

    bool m_id; // id of the timeline itself

    // what follows are some virtual function that corresponds to the QML. They are implemented in TimelineItemModel
protected:
    virtual void _beginRemoveRows(const QModelIndex &, int, int) = 0;
    virtual void _beginInsertRows(const QModelIndex &, int, int) = 0;
    virtual void _endRemoveRows() = 0;
    virtual void _endInsertRows() = 0;
    virtual void notifyChange(const QModelIndex &topleft, const QModelIndex &bottomright, bool start, bool duration, bool updateThumb) = 0;
    virtual void notifyChange(const QModelIndex &topleft, const QModelIndex &bottomright, QVector<int>) = 0;
    virtual QModelIndex makeClipIndexFromID(int) const = 0;
    virtual QModelIndex makeCompositionIndexFromID(int) const = 0;
    virtual QModelIndex makeTrackIndexFromID(int) const = 0;
    virtual void _resetView() = 0;
};
#endif
