"""
===========
Scheme Link
===========

"""

from PyQt4.QtCore import QObject
from PyQt4.QtCore import pyqtSignal as Signal
from PyQt4.QtCore import pyqtProperty as Property

from .utils import name_lookup
from .errors import IncompatibleChannelTypeError


def compatible_channels(source_channel, sink_channel):
    """Do the channels in link have compatible types, i.e. can they be
    connected based on their type.

    """
    source_type = name_lookup(source_channel.type)
    sink_type = name_lookup(sink_channel.type)
    ret = issubclass(source_type, sink_type)
    if source_channel.dynamic:
        ret = ret or issubclass(sink_type, source_type)
    return ret


def can_connect(source_node, sink_node):
    """Return True if any output from `source_node` can be connected to
    any input of `sink_node`.

    """
    return bool(possible_links(source_node, sink_node))


def possible_links(source_node, sink_node):
    """Return a list of (OutputSignal, InputSignal) tuples, that
    can connect the two nodes.

    """
    possible = []
    for source in source_node.output_channels():
        for sink in sink_node.input_channels():
            if compatible_channels(source, sink):
                possible.append((source, sink))
    return possible


class SchemeLink(QObject):
    """A instantiation of a link between two widget nodes in the scheme.

    Parameters
    ----------
    source_node : `SchemeNode`
        Source node.
    source_channel : `OutputSignal`
        The source widget's signal.
    sink_node : `SchemeNode`
        The sink node.
    sink_channel : `InputSignal`
        The sink widget's input signal.
    properties : `dict`
        Additional link properties.

    """

    enabled_changed = Signal(bool)
    dynamic_enabled_changed = Signal(bool)

    def __init__(self, source_node, source_channel,
                 sink_node, sink_channel, enabled=True, properties=None,
                 parent=None):
        QObject.__init__(self, parent)
        self.source_node = source_node

        if isinstance(source_channel, basestring):
            source_channel = source_node.output_channel(source_channel)
        elif source_channel not in source_node.output_channels():
            raise ValueError("%r not in in nodes output channels." \
                             % source_channel)

        self.source_channel = source_channel

        self.sink_node = sink_node

        if isinstance(sink_channel, basestring):
            sink_channel = sink_node.input_channel(sink_channel)
        elif sink_channel not in sink_node.input_channels():
            raise ValueError("%r not in in nodes input channels." \
                             % source_channel)

        self.sink_channel = sink_channel

        if not compatible_channels(source_channel, sink_channel):
            raise IncompatibleChannelTypeError(
                    "Cannot connect %r to %r" \
                    % (source_channel, sink_channel)
                )

        self.__enabled = enabled
        self.__dynamic_enabled = False
        self.__tool_tip = ""
        self.properties = properties or {}

    def source_type(self):
        """Return the type of the source channel.
        """
        return name_lookup(self.source_channel.type)

    def sink_type(self):
        """Return the type of the sink channel.
        """
        return name_lookup(self.sink_channel.type)

    def is_dynamic(self):
        """Is this link dynamic.
        """
        return self.source_channel.dynamic and \
            issubclass(self.sink_type(), self.source_type())

    def enabled(self):
        """Is this link enabled.
        """
        return self.__enabled

    def set_enabled(self, enabled):
        """Enable/disable the link.
        """
        if self.__enabled != enabled:
            self.__enabled = enabled
            self.enabled_changed.emit(enabled)

    enabled = Property(bool, fget=enabled, fset=set_enabled)

    def set_dynamic_enabled(self, enabled):
        """Enable/disable the dynamic link. Has no effect if
        the link is not dynamic.

        """
        if self.is_dynamic() and self.__dynamic_enabled != enabled:
            self.__dynamic_enabled = enabled
            self.dynamic_enabled_changed.emit(enabled)

    def dynamic_enabled(self):
        """Is this dynamic link and `dynamic_enabled` set to True
        """
        return self.is_dynamic() and self.__dynamic_enabled

    dynamic_enabled = Property(bool, fget=dynamic_enabled,
                                 fset=set_dynamic_enabled)

    def set_tool_tip(self, tool_tip):
        """Set the link tool tip.
        """
        if self.__tool_tip != tool_tip:
            self.__tool_tip = tool_tip

    def tool_tip(self):
        """Return the link's tool tip
        """
        return self.__tool_tip

    tool_tip = Property(str, fget=tool_tip,
                          fset=set_tool_tip)
