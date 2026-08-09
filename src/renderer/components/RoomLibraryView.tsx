import { ArrowLeft, Plus } from 'lucide-react';
import { useMemo, useState } from 'react';
import { useWorkspace } from '../store/workspace-context';
import { getRoomIdsForMode } from '../store/room-library';
import { RoomAvatar } from './RoomAvatar';

interface RoomLibraryViewProps {
  mode: 'favorites' | 'history';
  onBack: () => void;
}

export function RoomLibraryView({ mode, onBack }: RoomLibraryViewProps) {
  const roomLibrary = useWorkspace((state) => state.roomLibrary);
  const favoriteRoomIds = useWorkspace((state) => state.favoriteRoomIds);
  const history = useWorkspace((state) => state.history);
  const activeRooms = useWorkspace((state) => state.rooms);
  const addRoom = useWorkspace((state) => state.addRoom);
  const [message, setMessage] = useState<string>();
  const activeRoomIds = useMemo(
    () => activeRooms.map((room) => room.roomId),
    [activeRooms],
  );
  const roomIds = useMemo(
    () => getRoomIdsForMode(mode, favoriteRoomIds, history),
    [mode, favoriteRoomIds, history],
  );
  const rooms = useMemo(
    () => roomIds.flatMap((roomId) => roomLibrary[roomId] ? [roomLibrary[roomId]] : []),
    [roomIds, roomLibrary],
  );

  return (
    <div className="room-library-view">
      <div className="room-library-heading">
        <button className="tiny-icon-button" type="button" aria-label="返回当前直播间" title="返回" onClick={onBack}>
          <ArrowLeft size={15} />
        </button>
        <strong>{mode === 'favorites' ? '收藏' : '历史'}</strong>
        <span>{rooms.length}</span>
      </div>
      {message ? <p className="room-library-message" role="status">{message}</p> : null}
      <div className="room-library-list" role="list">
        {rooms.length ? rooms.map((room) => {
          const isActive = activeRoomIds.includes(room.roomId);
          return (
            <div className="library-room-row" role="listitem" key={room.roomId}>
              <RoomAvatar anchorName={room.anchorName} avatarUrl={room.avatarUrl} size="small" />
              <span className="library-room-copy">
                <strong>{room.anchorName}</strong>
                <small>
                  <span className={`status-dot ${room.online ? 'status-dot-live' : 'status-dot-offline'}`} />
                  <span className="library-room-status">{room.online ? '直播中' : '未开播'}</span>
                  <span className="library-room-category">{room.category}</span>
                </small>
              </span>
              <button
                className="tiny-icon-button"
                type="button"
                aria-label={isActive ? `${room.anchorName} 已添加` : `添加 ${room.anchorName}`}
                title={isActive ? '已添加' : '添加到当前画面'}
                disabled={isActive}
                onClick={() => {
                  const result = addRoom(room);
                  setMessage(result === 'added' ? `已添加 ${room.anchorName}` : '当前画面已达到 9 个房间');
                }}
              >
                <Plus size={14} />
              </button>
            </div>
          );
        }) : (
          <p className="room-library-empty">{mode === 'favorites' ? '还没有收藏直播间' : '还没有添加历史'}</p>
        )}
      </div>
    </div>
  );
}
