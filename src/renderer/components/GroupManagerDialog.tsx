import { ChevronDown, ChevronUp, Plus, Trash2, X } from 'lucide-react';
import { useEffect, useMemo, useState, type FormEvent } from 'react';
import { useWorkspace } from '../store/workspace-context';
import { RoomAvatar } from './RoomAvatar';

interface GroupManagerDialogProps {
  open: boolean;
  onClose: () => void;
}

export function GroupManagerDialog({ open, onClose }: GroupManagerDialogProps) {
  const groups = useWorkspace((state) => state.groups);
  const roomLibrary = useWorkspace((state) => state.roomLibrary);
  const createGroup = useWorkspace((state) => state.createGroup);
  const renameGroup = useWorkspace((state) => state.renameGroup);
  const deleteGroup = useWorkspace((state) => state.deleteGroup);
  const addRoomToGroup = useWorkspace((state) => state.addRoomToGroup);
  const removeRoomFromGroup = useWorkspace((state) => state.removeRoomFromGroup);
  const moveGroupRoom = useWorkspace((state) => state.moveGroupRoom);
  const [selectedGroupId, setSelectedGroupId] = useState<string>();
  const [newGroupName, setNewGroupName] = useState('');
  const [renameValue, setRenameValue] = useState('');
  const [message, setMessage] = useState<string>();
  const [deleteArmed, setDeleteArmed] = useState(false);
  const selectedGroup = groups.find((group) => group.id === selectedGroupId) ?? groups[0];
  const libraryRooms = useMemo(
    () => Object.values(roomLibrary).sort((left, right) => left.anchorName.localeCompare(right.anchorName, 'zh-CN')),
    [roomLibrary],
  );

  useEffect(() => {
    if (!open) return undefined;
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [onClose, open]);

  useEffect(() => {
    setRenameValue(selectedGroup?.name ?? '');
    setDeleteArmed(false);
    setMessage(undefined);
  }, [selectedGroup?.id, selectedGroup?.name]);

  if (!open) return null;

  function handleCreate(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    const id = createGroup(newGroupName);
    if (!id) {
      setMessage('请输入有效的分组名称');
      return;
    }
    setSelectedGroupId(id);
    setNewGroupName('');
    setMessage('分组已创建');
  }

  return (
    <div className="dialog-backdrop" role="presentation" onMouseDown={(event) => { if (event.target === event.currentTarget) onClose(); }}>
      <section className="group-manager-dialog" role="dialog" aria-modal="true" aria-labelledby="group-manager-title">
        <div className="dialog-heading">
          <div>
            <p className="section-kicker">ROOM GROUPS</p>
            <h2 id="group-manager-title">管理分组</h2>
          </div>
          <button className="icon-button icon-button-muted" type="button" aria-label="关闭分组管理" title="关闭" onClick={onClose}>
            <X size={18} />
          </button>
        </div>
        <div className="group-manager-body">
          <div className="group-manager-nav">
            <form className="group-create-form" onSubmit={handleCreate}>
              <input value={newGroupName} onChange={(event) => setNewGroupName(event.target.value)} placeholder="新分组名称" aria-label="新分组名称" />
              <button className="icon-button" type="submit" aria-label="新建分组" title="新建分组"><Plus size={16} /></button>
            </form>
            <div className="group-manager-list" role="list">
              {groups.map((group) => (
                <button
                  className={`group-manager-list-item ${selectedGroup?.id === group.id ? 'is-active' : ''}`}
                  type="button"
                  role="listitem"
                  key={group.id}
                  onClick={() => setSelectedGroupId(group.id)}
                >
                  <span>{group.name}</span><small>{group.roomIds.length} / 9</small>
                </button>
              ))}
            </div>
          </div>
          <div className="group-manager-content">
            {selectedGroup ? (
              <>
                <div className="group-edit-row">
                  <input value={renameValue} onChange={(event) => setRenameValue(event.target.value)} aria-label="分组名称" />
                  <button className="button" type="button" onClick={() => {
                    setMessage(renameGroup(selectedGroup.id, renameValue) ? '分组名称已保存' : '分组名称不能为空');
                  }}>保存名称</button>
                  <button className="button button-danger" type="button" onClick={() => {
                    if (!deleteArmed) {
                      setDeleteArmed(true);
                      setMessage('再次点击确认删除分组');
                      return;
                    }
                    deleteGroup(selectedGroup.id);
                    setSelectedGroupId(undefined);
                    setMessage('分组已删除，直播间资料仍然保留');
                  }}><Trash2 size={14} />{deleteArmed ? '确认删除' : '删除'}</button>
                </div>
                <p className="group-manager-hint">选择该分组包含的直播间，最多 9 个。</p>
                <div className="group-member-list">
                  {libraryRooms.length ? libraryRooms.map((room) => {
                    const index = selectedGroup.roomIds.indexOf(room.roomId);
                    const included = index >= 0;
                    return (
                      <div className="group-member-row" key={room.roomId}>
                        <label>
                          <input
                            type="checkbox"
                            checked={included}
                            onChange={() => {
                              if (included) {
                                removeRoomFromGroup(selectedGroup.id, room.roomId);
                                setMessage(`已从分组移除 ${room.anchorName}`);
                                return;
                              }
                              const result = addRoomToGroup(selectedGroup.id, room.roomId);
                              setMessage(result === 'added' ? `已加入 ${room.anchorName}` : '每个分组最多 9 个直播间');
                            }}
                          />
                          <RoomAvatar anchorName={room.anchorName} avatarUrl={room.avatarUrl} size="small" />
                          <span><strong>{room.anchorName}</strong><small>{room.category}</small></span>
                        </label>
                        {included ? (
                          <span className="group-member-actions">
                            <button className="tiny-icon-button" type="button" aria-label={`将 ${room.anchorName} 上移`} disabled={index === 0} onClick={() => moveGroupRoom(selectedGroup.id, room.roomId, -1)}><ChevronUp size={13} /></button>
                            <button className="tiny-icon-button" type="button" aria-label={`将 ${room.anchorName} 下移`} disabled={index === selectedGroup.roomIds.length - 1} onClick={() => moveGroupRoom(selectedGroup.id, room.roomId, 1)}><ChevronDown size={13} /></button>
                          </span>
                        ) : null}
                      </div>
                    );
                  }) : <p className="room-library-empty">先添加直播间，再把它们加入分组。</p>}
                </div>
              </>
            ) : <p className="group-manager-empty">新建一个分组开始整理直播间。</p>}
          </div>
        </div>
        {message ? <p className="group-manager-message" role="status">{message}</p> : null}
      </section>
    </div>
  );
}
