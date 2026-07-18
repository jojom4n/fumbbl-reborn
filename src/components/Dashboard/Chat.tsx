// =============================================================================
// Chat — Chat panel for bottom bar
// =============================================================================

import { useState, useRef, useEffect } from 'react';
import { ChatMessage } from '../../types/bloodbowl';

interface ChatProps {
  messages: ChatMessage[];
  onSend?: (text: string) => void;
}

export default function Chat({ messages, onSend }: ChatProps) {
  const [input, setInput] = useState('');
  const messagesEndRef = useRef<HTMLDivElement>(null);
  const inputRef = useRef<HTMLInputElement>(null);

  // Auto-scroll to bottom
  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages]);

  const handleSend = () => {
    if (input.trim()) {
      onSend?.(input.trim());
      setInput('');
      inputRef.current?.focus();
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  };

  return (
    <div className="w-1/3 bg-gray-900 border-r border-gray-700 flex flex-col rounded-lg overflow-hidden">
      {/* Header */}
      <div className="px-3 py-2 border-b border-gray-700 flex items-center gap-2">
        <span className="text-lg">💬</span>
        <span className="text-sm font-bold text-white">Chat</span>
      </div>

      {/* Messages */}
      <div className="flex-1 overflow-y-auto scrollbar-thin scrollbar-thumb-gray-700 scrollbar-track-gray-900 p-2 space-y-1">
        {messages.length > 0 ? (
          messages.map((msg) => (
            <div
              key={msg.id}
              className={`text-xs px-2 py-1 rounded ${
                msg.type === 'system'
                  ? 'bg-gray-800/50 text-gray-400 italic'
                  : msg.type === 'team'
                    ? 'bg-blue-900/20'
                    : 'bg-gray-800/30'
              }`}
            >
              {/* Sender */}
              <span
                className="font-bold"
                style={{ color: msg.senderColor || '#9ca3af' }}
              >
                {msg.sender}:
              </span>
              {/* Message */}
              <span className="ml-1 text-gray-300">{msg.text}</span>
            </div>
          ))
        ) : (
          <div className="text-center text-gray-500 py-4 text-xs">
            No messages yet
          </div>
        )}
        <div ref={messagesEndRef} />
      </div>

      {/* Input */}
      <div className="px-2 py-2 border-t border-gray-700 flex gap-1">
        <input
          ref={inputRef}
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onKeyDown={handleKeyDown}
          placeholder="Type a message..."
          className="flex-1 bg-gray-800 border border-gray-600 rounded px-2 py-1 text-xs text-white placeholder-gray-500 focus:outline-none focus:border-gray-400"
        />
        <button
          onClick={handleSend}
          disabled={!input.trim()}
          className={`
            px-3 py-1 rounded text-xs font-medium transition-colors
            ${input.trim()
              ? 'bg-blue-600 text-white hover:bg-blue-500'
              : 'bg-gray-700 text-gray-500 cursor-not-allowed'
            }
          `}
        >
          Send
        </button>
      </div>
    </div>
  );
}