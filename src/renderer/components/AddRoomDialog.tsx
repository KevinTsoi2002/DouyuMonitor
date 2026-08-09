import { AlertCircle, Check, LoaderCircle, Search, X } from 'lucide-react';
import { useEffect, useState, type FormEvent } from 'react';
import { useWorkspace } from '../store/workspace-context';
import { RoomAvatar } from './RoomAvatar';

interface AddRoomDialogProps {
  open: boolean;
  onClose: () => void;
}

export function AddRoomDialog({ open, onClose }: AddRoomDialogProps) {
  const [input, setInput] = useState('');
  const [actionError, setActionError] = useState<string>();
  const searchRooms = useWorkspace((state) => state.searchRooms);
  const searchResults = useWorkspace((state) => state.searchResults);
  const searchStatus = useWorkspace((state) => state.searchStatus);
  const searchError = useWorkspace((state) => state.searchError);
  const addRoom = useWorkspace((state) => state.addRoom);
  const demoMode = useWorkspace((state) => state.demoMode);

  useEffect(() => {
    if (!open) return undefined;
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') onClose();
    };
    window.addEventListener('keydown', onKeyDown);
    return () => window.removeEventListener('keydown', onKeyDown);
  }, [onClose, open]);

  if (!open) return null;

  async function handleSubmit(event: FormEvent<HTMLFormElement>) {
    event.preventDefault();
    setActionError(undefined);
    await searchRooms(input);
  }

  function handleAdd(room: (typeof searchResults)[number]) {
    const result = addRoom(room);
    if (result === 'added') {
      onClose();
      setInput('');
      return;
    }
    setActionError(result === 'duplicate' ? '这个直播间已经在直播间列表里' : '最多同时监看 9 个直播间');
  }

  const message = actionError ?? searchError;

  return (
    <div className="dialog-backdrop" role="presentation" onMouseDown={(event) => { if (event.target === event.currentTarget) onClose(); }}>
      <section className="add-dialog" role="dialog" aria-modal="true" aria-labelledby="add-dialog-title">
        <div className="dialog-heading">
          <div>
            <p className="section-kicker">NEW SOURCE</p>
            <h2 id="add-dialog-title">添加直播间</h2>
          </div>
          <button className="icon-button icon-button-muted" type="button" aria-label="关闭添加直播间窗口" title="关闭" onClick={onClose}><X size={18} /></button>
        </div>
        <p className="dialog-description">输入直播间号、斗鱼链接或主播名字，搜索后加入监看工作区。</p>
        <form className="room-search-form" onSubmit={handleSubmit}>
          <label className="sr-only" htmlFor="room-search">直播间号或主播名字</label>
          <div className="search-input-wrap">
            <Search size={17} aria-hidden="true" />
            <input id="room-search" value={input} onChange={(event) => setInput(event.target.value)} placeholder="例如：63136 或 星河" autoFocus />
          </div>
          <button className="button button-primary" type="submit" disabled={searchStatus === 'searching'}>
            {searchStatus === 'searching' ? <LoaderCircle size={16} className="spin" /> : <Search size={16} />}
            <span>搜索</span>
          </button>
        </form>

        {message ? <p className="dialog-message dialog-message-error"><AlertCircle size={15} />{message}</p> : null}
        {searchStatus === 'empty' ? <p className="dialog-message dialog-message-muted">没有找到匹配的直播间，请检查输入。</p> : null}

        {searchResults.length ? (
          <div className="search-results" aria-live="polite">
            <div className="results-heading"><span>搜索结果</span><small>{searchResults.length} 个候选</small></div>
            {searchResults.map((room) => (
              <button className="search-result" type="button" key={room.roomId} onClick={() => handleAdd(room)}>
                <RoomAvatar anchorName={room.anchorName} avatarUrl={room.avatarUrl} size="small" />
                <span className="search-result-copy"><strong>{room.anchorName}</strong><span>{room.title}</span><small>{room.roomId} · {room.category}</small></span>
                <span className="result-add"><Check size={15} /> 加入</span>
              </button>
            ))}
          </div>
        ) : null}
        <div className="dialog-note">
          <span className="status-dot status-dot-live" />
          {demoMode
            ? '当前为模拟数据，真实斗鱼连接将在后续阶段接入。'
            : '房间信息来自斗鱼公开接口，播放源按合规规则检查。'}
        </div>
      </section>
    </div>
  );
}
