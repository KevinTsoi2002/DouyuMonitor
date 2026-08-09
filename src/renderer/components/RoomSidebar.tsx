import {
  ChevronDown,
  ChevronUp,
  Clock3,
  FolderCog,
  FolderPlus,
  GripVertical,
  Headphones,
  MoreHorizontal,
  Plus,
  Star,
  Trash2,
  Volume2,
  VolumeX,
} from 'lucide-react';
import { useState } from 'react';
import { useWorkspace } from '../store/workspace-context';
import { getPlaybackPresentation } from '../ui-model';
import { RoomAvatar } from './RoomAvatar';
import { RoomLibraryView } from './RoomLibraryView';

interface RoomSidebarProps {
  onAddRoom: () => void;
  onManageGroups: () => void;
  isOpen: boolean;
}

type SidebarView = 'current' | 'favorites' | 'history';

export function RoomSidebar({ onAddRoom, onManageGroups, isOpen }: RoomSidebarProps) {
  const rooms = useWorkspace((state) => state.rooms);
  const groups = useWorkspace((state) => state.groups);
  const activeGroupId = useWorkspace((state) => state.activeGroupId);
  const favoriteRoomIds = useWorkspace((state) => state.favoriteRoomIds);
  const primaryRoomId = useWorkspace((state) => state.primaryRoomId);
  const audioRoomId = useWorkspace((state) => state.audioRoomId);
  const switchGroup = useWorkspace((state) => state.switchGroup);
  const toggleFavorite = useWorkspace((state) => state.toggleFavorite);
  const setPrimaryRoom = useWorkspace((state) => state.setPrimaryRoom);
  const setAudioRoom = useWorkspace((state) => state.setAudioRoom);
  const moveRoom = useWorkspace((state) => state.moveRoom);
  const reorderRooms = useWorkspace((state) => state.reorderRooms);
  const removeRoom = useWorkspace((state) => state.removeRoom);
  const [view, setView] = useState<SidebarView>('current');
  const [moreGroupsOpen, setMoreGroupsOpen] = useState(false);
  const [draggingRoomId, setDraggingRoomId] = useState<string>();
  const visibleGroups = groups.slice(0, 3);
  const overflowGroups = groups.slice(3);

  function activateGroup(groupId: string) {
    switchGroup(groupId);
    setView('current');
    setMoreGroupsOpen(false);
  }

  return (
    <aside
      className={`room-sidebar ${isOpen ? 'is-open' : 'is-closed'}`}
      aria-label="直播间列表"
      aria-hidden={!isOpen}
    >
      <div className="sidebar-heading">
        <h1>直播间列表</h1>
        <button className="icon-button icon-button-muted" type="button" aria-label="添加直播间" title="添加直播间" onClick={onAddRoom}>
          <Plus size={17} />
        </button>
      </div>

      <div className="group-tabs" aria-label="直播间分组">
        {visibleGroups.map((group) => (
          <button
            className={`group-tab ${view === 'current' && activeGroupId === group.id ? 'is-active' : ''}`}
            type="button"
            aria-pressed={view === 'current' && activeGroupId === group.id}
            key={group.id}
            onClick={() => activateGroup(group.id)}
          >{group.name}</button>
        ))}
        {overflowGroups.length ? (
          <div className="group-more-wrap">
            <button className="group-tab group-more-trigger" type="button" aria-label="更多分组" aria-expanded={moreGroupsOpen} onClick={() => setMoreGroupsOpen((open) => !open)}>
              <MoreHorizontal size={14} />
            </button>
            {moreGroupsOpen ? (
              <div className="group-more-menu" role="menu" aria-label="更多直播间分组">
                {overflowGroups.map((group) => (
                  <button type="button" role="menuitem" key={group.id} onClick={() => activateGroup(group.id)}>{group.name}</button>
                ))}
              </div>
            ) : null}
          </div>
        ) : null}
        <button className="group-tab group-manage-trigger" type="button" aria-label="管理分组" title="管理分组" onClick={onManageGroups}>
          <FolderCog size={14} />
        </button>
      </div>

      <div className="library-tabs" role="tablist" aria-label="快速添加">
        <button className={`library-tab ${view === 'favorites' ? 'is-active' : ''}`} type="button" role="tab" aria-selected={view === 'favorites'} onClick={() => setView('favorites')}><Star size={13} />收藏</button>
        <button className={`library-tab ${view === 'history' ? 'is-active' : ''}`} type="button" role="tab" aria-selected={view === 'history'} onClick={() => setView('history')}><Clock3 size={13} />历史</button>
      </div>

      {view !== 'current' ? (
        <RoomLibraryView mode={view} onBack={() => setView('current')} />
      ) : (
        <div className="room-list" role="list">
          {rooms.length ? rooms.map((room, index) => {
            const isPrimary = room.roomId === primaryRoomId;
            const isAudio = room.roomId === audioRoomId;
            const isFavorite = favoriteRoomIds.includes(room.roomId);
            const audioDisabled = getPlaybackPresentation(room).audioDisabled;
            const hasAudioFocus = isAudio && !audioDisabled;
            const online = room.online && room.status !== 'offline';
            return (
              <div
                className={`room-row ${isPrimary ? 'is-primary' : ''} ${draggingRoomId === room.roomId ? 'is-dragging' : ''}`}
                key={room.roomId}
                role="listitem"
                draggable
                onDragStart={(event) => {
                  setDraggingRoomId(room.roomId);
                  event.dataTransfer.effectAllowed = 'move';
                  event.dataTransfer.setData('text/plain', room.roomId);
                }}
                onDragOver={(event) => event.preventDefault()}
                onDrop={(event) => {
                  event.preventDefault();
                  const sourceRoomId = event.dataTransfer.getData('text/plain') || draggingRoomId;
                  if (sourceRoomId) reorderRooms(sourceRoomId, room.roomId);
                  setDraggingRoomId(undefined);
                }}
                onDragEnd={() => setDraggingRoomId(undefined)}
              >
                <span className="room-row-drag-handle" aria-hidden="true" title="拖拽排序"><GripVertical size={15} /></span>
                <button className="room-row-main" type="button" onClick={() => setPrimaryRoom(room.roomId)} aria-pressed={isPrimary}>
                  <RoomAvatar anchorName={room.anchorName} avatarUrl={room.avatarUrl} />
                  <span className="room-row-copy">
                    <span className="room-row-name">{room.anchorName}</span>
                    <span className="room-row-meta">
                      <span
                        className={`status-dot ${online ? 'status-dot-live' : 'status-dot-offline'}`}
                        role="img"
                        aria-label={online ? '直播中' : '未开播'}
                        title={online ? '直播中' : '未开播'}
                      />
                      {isPrimary ? <span className="primary-tag room-row-primary">主</span> : null}
                    </span>
                  </span>
                </button>
                <div className="room-row-actions">
                  <button className={`tiny-icon-button ${isFavorite ? 'is-favorite' : ''}`} type="button" aria-label={isFavorite ? `取消收藏 ${room.anchorName}` : `收藏 ${room.anchorName}`} title={isFavorite ? '取消收藏' : '收藏'} onClick={() => toggleFavorite(room.roomId)}><Star size={13} fill={isFavorite ? 'currentColor' : 'none'} /></button>
                  <button className="tiny-icon-button" type="button" aria-label={`将 ${room.anchorName} 加入分组`} title="加入分组" onClick={onManageGroups}><FolderPlus size={13} /></button>
                  <button className="tiny-icon-button" type="button" aria-label={`将 ${room.anchorName} 上移`} title="上移" disabled={index === 0} onClick={() => moveRoom(room.roomId, -1)}><ChevronUp size={13} /></button>
                  <button className="tiny-icon-button" type="button" aria-label={`将 ${room.anchorName} 下移`} title="下移" disabled={index === rooms.length - 1} onClick={() => moveRoom(room.roomId, 1)}><ChevronDown size={13} /></button>
                  <button className={`tiny-icon-button ${hasAudioFocus ? 'is-audio' : ''}`} type="button" aria-label={audioDisabled ? '暂无可用音频' : hasAudioFocus ? `关闭 ${room.anchorName} 声音` : `只播放 ${room.anchorName}`} title={audioDisabled ? '暂无可用音频' : hasAudioFocus ? '关闭声音' : '切换声音'} disabled={audioDisabled} onClick={() => setAudioRoom(hasAudioFocus ? undefined : room.roomId)}>
                    {hasAudioFocus ? <Volume2 size={13} /> : <VolumeX size={13} />}
                  </button>
                  <button className="tiny-icon-button danger" type="button" aria-label={`移除 ${room.anchorName}`} title="移除" onClick={() => removeRoom(room.roomId)}><Trash2 size={13} /></button>
                </div>
              </div>
            );
          }) : (
            <div className="sidebar-empty">
              <Headphones size={20} />
              <p>还没有添加直播间</p>
              <span>添加房间或切换到一个分组</span>
            </div>
          )}
        </div>
      )}

      <button className="sidebar-manage-button" type="button" onClick={onManageGroups}><FolderCog size={14} />管理分组</button>
    </aside>
  );
}
