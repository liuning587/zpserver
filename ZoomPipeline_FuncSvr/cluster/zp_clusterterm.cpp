#include "zp_clusterterm.h"
#include "zp_clusternode.h"
#include <assert.h>
namespace ZP_Cluster{
	zp_ClusterTerm::zp_ClusterTerm(const QString & name,QObject *parent ) :
		QObject(parent)
	  ,m_strTermName(name)
	{
		m_pClusterEng = new ZPTaskEngine::zp_pipeline(this);
		m_pClusterNet = new ZPNetwork::zp_net_ThreadPool(8192,this);
		connect(m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_Message, this,&zp_ClusterTerm::evt_Message);
		connect(m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_SocketError, this,&zp_ClusterTerm::evt_SocketError);
		connect(m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_Data_recieved, this,&zp_ClusterTerm::on_evt_Data_recieved);
		connect(m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_Data_transferred, this,&zp_ClusterTerm::on_evt_Data_transferred);
		connect(m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_ClientDisconnected, this,&zp_ClusterTerm::on_evt_ClientDisconnected);
		connect(m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_NewClientConnected, this,&zp_ClusterTerm::on_evt_NewClientConnected);
		//connect(m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_ClientEncrypted, this,&zp_ClusterTerm::on_evt_ClientEncrypted);
		m_nPortPublish = 0;
		m_nHeartBeatingDeadThrd = 20;

	}
	void zp_ClusterTerm::StartListen(const QHostAddress &addr, int nPort)
	{
		m_pClusterNet->AddListeningAddress(m_strTermName,addr,nPort,false);

	}
	bool zp_ClusterTerm::JoinCluster(const QHostAddress &addr, int nPort,bool bSSL)
	{
		return m_pClusterNet->connectTo(addr,nPort,bSSL);
	}
	bool zp_ClusterTerm::canExit()
	{
		return m_pClusterEng->canClose() && m_pClusterNet->CanExit();
	}

	bool zp_ClusterTerm::regisitNewServer(zp_ClusterNode * c)
	{
		//Before reg, termname must be recieved.
		if (c->termName().length()<1)
			return false;
		m_hash_mutex.lock();
		m_hash_Name2node[c->termName()] = c;
		m_hash_mutex.unlock();
		return true;
	}

	zp_ClusterNode * zp_ClusterTerm::SvrNodeFromName(const QString & uuid)
	{
		m_hash_mutex.lock();
		if (m_hash_Name2node.contains(uuid))
		{
			m_hash_mutex.unlock();
			return m_hash_Name2node[uuid];
		}
		m_hash_mutex.unlock();

		return NULL;
	}

	zp_ClusterNode * zp_ClusterTerm::SvrNodeFromSocket(QObject * sock)
	{
		m_hash_mutex.lock();
		if (m_hash_sock2node.contains(sock))
		{
			m_hash_mutex.unlock();
			return m_hash_sock2node[sock];
		}
		m_hash_mutex.unlock();
		return NULL;
	}

	//this event indicates new client connected.
	void  zp_ClusterTerm::on_evt_NewClientConnected(QObject * clientHandle)
	{
		bool nHashContains = false;
		zp_ClusterNode * pClientNode = 0;
		m_hash_mutex.lock();
		nHashContains = m_hash_sock2node.contains(clientHandle);
		if (false==nHashContains)
		{
			zp_ClusterNode * pnode = new zp_ClusterNode(this,clientHandle,0);
			//using queued connection of send and revieve;
			connect (pnode,&zp_ClusterNode::evt_SendDataToClient,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::SendDataToClient,Qt::QueuedConnection);
			connect (pnode,&zp_ClusterNode::evt_BroadcastData,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_BroadcastData,Qt::QueuedConnection);
			connect (pnode,&zp_ClusterNode::evt_close_client,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::KickClients,Qt::QueuedConnection);
			connect (pnode,&zp_ClusterNode::evt_Message,this,&zp_ClusterTerm::evt_Message,Qt::QueuedConnection);
			m_hash_sock2node[clientHandle] = pnode;
			nHashContains = true;
			pClientNode = pnode;
		}
		else
		{
			pClientNode =  m_hash_sock2node[clientHandle];
		}
		m_hash_mutex.unlock();
		assert(nHashContains!=0 && pClientNode !=0);
	}

	//this event indicates new client encrypted.
	void  zp_ClusterTerm::on_evt_ClientEncrypted(QObject * clientHandle)
	{
		bool nHashContains = false;
		zp_ClusterNode * pClientNode = 0;
		m_hash_mutex.lock();
		nHashContains = m_hash_sock2node.contains(clientHandle);
		if (false==nHashContains)
		{
			zp_ClusterNode * pnode = new zp_ClusterNode(this,clientHandle,0);
			//using queued connection of send and revieve;
			connect (pnode,&zp_ClusterNode::evt_SendDataToClient,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::SendDataToClient,Qt::QueuedConnection);
			connect (pnode,&zp_ClusterNode::evt_BroadcastData,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_BroadcastData,Qt::QueuedConnection);
			connect (pnode,&zp_ClusterNode::evt_close_client,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::KickClients,Qt::QueuedConnection);
			connect (pnode,&zp_ClusterNode::evt_Message,this,&zp_ClusterTerm::evt_Message,Qt::QueuedConnection);
			m_hash_sock2node[clientHandle] = pnode;
			nHashContains = true;
			pClientNode = pnode;
		}
		else
		{
			pClientNode =  m_hash_sock2node[clientHandle];
		}
		m_hash_mutex.unlock();
		assert(nHashContains!=0 && pClientNode !=0);
	}

	//this event indicates a client disconnected.
	void  zp_ClusterTerm::on_evt_ClientDisconnected(QObject * clientHandle)
	{
		bool nHashContains  = false;
		zp_ClusterNode * pClientNode = 0;
		m_hash_mutex.lock();
		nHashContains = m_hash_sock2node.contains(clientHandle);
		if (nHashContains)
			pClientNode =  m_hash_sock2node[clientHandle];
		if (pClientNode)
		{
			m_hash_sock2node.remove(clientHandle);
			if (pClientNode->termName().length()>0)
				m_hash_Name2node.remove(pClientNode->termName());

			pClientNode->bTermSet = true;
			disconnect (pClientNode,&zp_ClusterNode::evt_SendDataToClient,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::SendDataToClient);
			disconnect (pClientNode,&zp_ClusterNode::evt_BroadcastData,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_BroadcastData);
			disconnect (pClientNode,&zp_ClusterNode::evt_close_client,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::KickClients);
			disconnect (pClientNode,&zp_ClusterNode::evt_Message,this,&zp_ClusterTerm::evt_Message);

			m_nodeToBeDel.push_back(pClientNode);
			//qDebug()<<QString("%1(ref %2) Node Push in queue.\n").arg((unsigned int)pClientNode).arg(pClientNode->ref());
		}
		m_hash_mutex.unlock();

		//Try to delete objects
		QList <zp_ClusterNode *> toBedel;
		foreach(zp_ClusterNode * pdelobj,m_nodeToBeDel)
		{
			if (pdelobj->ref() ==0)
				toBedel.push_back(pdelobj);
			else
			{
				//qDebug()<<QString("%1(ref %2) Waiting in del queue.\n").arg((unsigned int)pdelobj).arg(pdelobj->ref());
			}
		}
		foreach(zp_ClusterNode * pdelobj,toBedel)
		{
			m_nodeToBeDel.removeAll(pdelobj);
			//qDebug()<<QString("%1(ref %2) deleting.\n").arg((unsigned int)pdelobj).arg(pdelobj->ref());
			pdelobj->deleteLater();
		}
	}

	//some data arrival
	void  zp_ClusterTerm::on_evt_Data_recieved(QObject *  clientHandle,const QByteArray & datablock )
	{
		//Push Clients to nodes if it is not exist
		bool nHashContains = false;
		zp_ClusterNode * pClientNode = 0;
		m_hash_mutex.lock();
		nHashContains = m_hash_sock2node.contains(clientHandle);
		if (false==nHashContains)
		{
			zp_ClusterNode * pnode = new zp_ClusterNode(this,clientHandle,0);
			//using queued connection of send and revieve;
			connect (pnode,&zp_ClusterNode::evt_SendDataToClient,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::SendDataToClient,Qt::QueuedConnection);
			connect (pnode,&zp_ClusterNode::evt_BroadcastData,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::evt_BroadcastData,Qt::QueuedConnection);
			connect (pnode,&zp_ClusterNode::evt_close_client,m_pClusterNet,&ZPNetwork::zp_net_ThreadPool::KickClients,Qt::QueuedConnection);
			connect (pnode,&zp_ClusterNode::evt_Message,this,&zp_ClusterTerm::evt_Message,Qt::QueuedConnection);
			m_hash_sock2node[clientHandle] = pnode;
			nHashContains = true;
			pClientNode = pnode;
		}
		else
		{
			pClientNode =  m_hash_sock2node[clientHandle];
		}
		assert(nHashContains!=0 && pClientNode !=0);
		int nblocks =  pClientNode->push_new_data(datablock);
		if (nblocks<=1)
			m_pClusterEng->pushTask(pClientNode);
		m_hash_mutex.unlock();
	}
	void  zp_ClusterTerm::KickDeadClients()
	{
		m_hash_mutex.lock();
		for (QMap<QObject *,zp_ClusterNode *>::iterator p =m_hash_sock2node.begin();
			 p!=m_hash_sock2node.end();p++)
		{
			p.value()->CheckHeartBeating();
		}
		m_hash_mutex.unlock();
	}
	//a block of data has been successfuly sent
	void  zp_ClusterTerm::on_evt_Data_transferred(QObject *   /*clientHandle*/,qint64 /*bytes sent*/)
	{

	}
}
